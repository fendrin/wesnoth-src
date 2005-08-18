/* $Id$ */
/*
   Copyright (C) 2003-5 by David White <davidnwhite@verizon.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "log.hpp"
#include "network_worker.hpp"
#include "network.hpp"
#include "thread.hpp"
#include "wassert.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <vector>

#define LOG_NW LOG_STREAM(info, network)
#define ERR_NW LOG_STREAM(err, network)
namespace {

unsigned int buf_id = 0;

struct buffer {
	explicit buffer(TCPsocket sock) : sock(sock) {}

	TCPsocket sock;
	mutable std::vector<char> buf;
};

bool managed = false;
typedef std::vector< buffer * > buffer_set;
buffer_set bufs;

//a queue of sockets that we are waiting to receive on
typedef std::vector<TCPsocket> receive_list;
receive_list pending_receives;

typedef std::deque<buffer> received_queue;
received_queue received_data_queue;

enum SOCKET_STATE { SOCKET_READY, SOCKET_LOCKED, SOCKET_ERROR, SOCKET_INTERRUPT };
typedef std::map<TCPsocket,SOCKET_STATE> socket_state_map;
typedef std::map<TCPsocket, std::pair<network::statistics,network::statistics> > socket_stats_map;

socket_state_map sockets_locked;
socket_stats_map transfer_stats;

int socket_errors = 0;
threading::mutex* global_mutex = NULL;
threading::condition* cond = NULL;

std::vector<threading::thread*> threads;

SOCKET_STATE send_buf(TCPsocket sock, std::vector<char>& buf) {
	int timeout = 15000;
	size_t upto = 0;
	size_t size = buf.size();
	{
		const threading::lock lock(*global_mutex);
		transfer_stats[sock].first.fresh_current(size);
	}

	while(upto < size && timeout > 0) {
		{
			// check if the socket is still locked
			const threading::lock lock(*global_mutex);
			if(sockets_locked[sock] != SOCKET_LOCKED)
				return SOCKET_ERROR;
		}
		const int bytes_to_send = static_cast<int>(size - upto);
		const int res = SDLNet_TCP_Send(sock, &buf[upto], bytes_to_send);

		if(res == -1) {
#ifdef EAGAIN
			if(errno == EAGAIN) {
				SDL_Delay(100);
				timeout -= 100;
				continue;
			}
#elif defined(EWOULDBLOCK)
			if(errno == EWOULDBLOCK) {
				SDL_Delay(100);
				timeout -= 100;
				continue;
			}
#endif
			return SOCKET_ERROR;
		}
		if(res != bytes_to_send) {
			return SOCKET_ERROR;
		}
		timeout = 15000;
		upto += static_cast<size_t>(res);
		{
			const threading::lock lock(*global_mutex);
			transfer_stats[sock].first.transfer(static_cast<size_t>(res));
		}
	}
	return SOCKET_READY;
}

SOCKET_STATE receive_buf(TCPsocket sock, std::vector<char>& buf)
{
	SDLNet_SocketSet set = SDLNet_AllocSocketSet(1);
	if(!set) {
		ERR_NW << "SDLNet_AllocSocketSet: " << SDLNet_GetError() << "\n";
		SDLNet_FreeSocketSet(set);
		return SOCKET_ERROR;
	}
	if(SDLNet_TCP_AddSocket(set, sock) < 0) {
		ERR_NW << "SDLNet_TCP_AddSocket: " << SDLNet_GetError() << "\n";
		SDLNet_FreeSocketSet(set);
		return SOCKET_ERROR;
	}
	char num_buf[4];
	int len = SDLNet_TCP_Recv(sock,num_buf,4);

	if(len != 4) {
		SDLNet_TCP_DelSocket(set, sock);
		SDLNet_FreeSocketSet(set);
		return SOCKET_ERROR;
	}

	len = SDLNet_Read32(num_buf);

	if(len < 1 || len > 100000000) {
		SDLNet_TCP_DelSocket(set, sock);
		SDLNet_FreeSocketSet(set);
		return SOCKET_ERROR;
	}

	buf.resize(len);
	char* beg = &buf[0];
	const char* const end = beg + len;

	{
		const threading::lock lock(*global_mutex);
		transfer_stats[sock].second.fresh_current(len);
	}

	// wait for a maximum of 15 seconds for the socket to have activity
	if(SDLNet_CheckSockets(set, 15000) <= 0) {
		ERR_NW << "SDLNet_CheckSockets: " << strerror(errno) << "\n";
		SDLNet_TCP_DelSocket(set, sock);
		SDLNet_FreeSocketSet(set);
		return SOCKET_ERROR;
	}
	while(beg != end) {
		{
			// if we are receiving the socket is in sockets_locked
			// check if it is still locked
			const threading::lock lock(*global_mutex);
			if(sockets_locked[sock] != SOCKET_LOCKED) {
				SDLNet_TCP_DelSocket(set, sock);
				SDLNet_FreeSocketSet(set);
				return SOCKET_ERROR;
			}
		}

		const ssize_t res = SDLNet_TCP_Recv(sock, beg, end - beg);
		if(res <= 0) {
			if(SDLNet_CheckSockets(set, 15000) <= 0) {
				ERR_NW << "SDLNet_CheckSockets: " << strerror(errno) << "\n";
				SDLNet_TCP_DelSocket(set, sock);
				SDLNet_FreeSocketSet(set);
				return SOCKET_ERROR;
			}
			continue;
		}

		beg += res;
		{
			const threading::lock lock(*global_mutex);
			transfer_stats[sock].second.transfer(static_cast<size_t>(res));
		}
	}
	SDLNet_TCP_DelSocket(set, sock);
	SDLNet_FreeSocketSet(set);
	return SOCKET_READY;
}

int process_queue(void* data)
{
	LOG_NW << "thread started...\n";
	for(;;) {

		//if we find a socket to send data to, sent_buf will be non-NULL. If we find a socket
		//to receive data from, sent_buf will be NULL. 'sock' will always refer to the socket
		//that data is being sent to/received from
		TCPsocket sock = NULL;
		buffer *sent_buf = NULL;

		{
			const threading::lock lock(*global_mutex);

			for(;;) {

				buffer_set::iterator itor = bufs.begin(), itor_end = bufs.end();
				for(; itor != itor_end; ++itor) {
					socket_state_map::iterator lock_it = sockets_locked.find((*itor)->sock);
					wassert(lock_it != sockets_locked.end());
					if(lock_it->second == SOCKET_READY) {
						lock_it->second = SOCKET_LOCKED;
						sent_buf = *itor;
						sock = sent_buf->sock;
						bufs.erase(itor);
						break;
					}
				}

				if(sock == NULL) {
					receive_list::iterator itor = pending_receives.begin(), itor_end = pending_receives.end();
					for(; itor != itor_end; ++itor) {
						socket_state_map::iterator lock_it = sockets_locked.find(*itor);
						wassert(lock_it != sockets_locked.end());
						if(lock_it->second == SOCKET_READY) {
							lock_it->second = SOCKET_LOCKED;
							sock = *itor;
							pending_receives.erase(itor);
							break;
						}
					}
				}

				if(sock != NULL) {
					break;
				}

				if(managed == false) {
					LOG_NW << "worker thread exiting...\n";
					return 0;
				}

				cond->wait(*global_mutex); // temporarily release the mutex and wait for a buffer
			}
		}

		wassert(sock);

		LOG_NW << "thread found a buffer...\n";

		SOCKET_STATE result = SOCKET_READY;
		std::vector<char> buf;

		if(sent_buf != NULL) {
			result = send_buf(sent_buf->sock, sent_buf->buf);
			delete sent_buf;
			sent_buf = NULL;
		} else {
			result = receive_buf(sock,buf);
		}

		{
			const threading::lock lock(*global_mutex);
			socket_state_map::iterator lock_it = sockets_locked.find(sock);
			wassert(lock_it != sockets_locked.end());
			lock_it->second = result;
			if(result == SOCKET_ERROR) {
				++socket_errors;
			}

			//if we received data, add it to the queue
			if(result == SOCKET_READY && buf.empty() == false) {
				received_data_queue.push_back(buffer(sock));
				received_data_queue.back().buf.swap(buf);
			}
		}
	}
	// unreachable
}

}

namespace network_worker_pool
{

manager::manager(size_t nthreads) : active_(!managed)
{
	if(active_) {
		managed = true;
		global_mutex = new threading::mutex();
		cond = new threading::condition();

		for(size_t n = 0; n != nthreads; ++n) {
			threads.push_back(new threading::thread(process_queue,NULL));
		}
	}
}

manager::~manager()
{
	if(active_) {
		{
			const threading::lock lock(*global_mutex);
			managed = false;
			socket_errors = 0;
		}
		cond->notify_all();

		for(std::vector<threading::thread*>::const_iterator i = threads.begin(); i != threads.end(); ++i) {
			LOG_NW << "waiting for thread " << int(i - threads.begin()) << " to exit...\n";
			delete *i;
			LOG_NW << "thread exited...\n";
		}

		threads.clear();

		delete global_mutex;
		delete cond;
		global_mutex = NULL;
		cond = NULL;

		sockets_locked.clear();
		transfer_stats.clear();

		LOG_NW << "exiting manager::~manager()\n";
	}
}

void receive_data(TCPsocket sock)
{
	{
		const threading::lock lock(*global_mutex);

		pending_receives.push_back(sock);
		sockets_locked.insert(std::pair<TCPsocket,SOCKET_STATE>(sock,SOCKET_READY));
	}

	cond->notify_one();
}

TCPsocket get_received_data(TCPsocket sock, std::vector<char>& buf)
{
	const threading::lock lock(*global_mutex);
	received_queue::iterator itor = received_data_queue.begin();
	if(sock != NULL) {
		for(; itor != received_data_queue.end(); ++itor) {
			if(itor->sock == sock) {
				break;
			}
		}
	}

	if(itor == received_data_queue.end()) {
		return NULL;
	} else {
		buf.swap(itor->buf);
		const TCPsocket res = itor->sock;
		received_data_queue.erase(itor);
		return res;
	}
}

void queue_data(TCPsocket sock, std::vector<char>& buf)
{
	LOG_NW << "queuing " << buf.size() << " bytes of data...\n";

	{
		const threading::lock lock(*global_mutex);

		buffer *queued_buf = new buffer(sock);
		queued_buf->buf.swap(buf);
		bufs.push_back(queued_buf);

		sockets_locked.insert(std::pair<TCPsocket,SOCKET_STATE>(sock,SOCKET_READY));
	}

	cond->notify_one();
}

namespace
{

void remove_buffers(TCPsocket sock)
{
	buffer_set new_bufs;
	new_bufs.reserve(bufs.size());
	for(buffer_set::iterator i = bufs.begin(), i_end = bufs.end(); i != i_end; ++i) {
		if ((*i)->sock == sock)
			delete *i;
		else
			new_bufs.push_back(*i);
	}
	bufs.swap(new_bufs);

	for(received_queue::iterator j = received_data_queue.begin(); j != received_data_queue.end(); ) {
		if(j->sock == sock) {
			j = received_data_queue.erase(j);
		} else {
			++j;
		}
	}
}

}

void close_socket(TCPsocket sock)
{
	for(bool first_time = true; ; first_time = false) {
		if(!first_time) {
			SDL_Delay(10);
		}

		const threading::lock lock(*global_mutex);

		if(first_time) {
			pending_receives.erase(std::remove(pending_receives.begin(),pending_receives.end(),sock),pending_receives.end());
		}

		const socket_state_map::iterator lock_it = sockets_locked.find(sock);
		if(lock_it == sockets_locked.end()) {
			remove_buffers(sock);
			break;
		}
			
		if(lock_it->second != SOCKET_LOCKED) {
			if(lock_it->second != SOCKET_INTERRUPT) {
				sockets_locked.erase(lock_it);
				remove_buffers(sock);
				break;
			}
		} else {
			lock_it->second = SOCKET_INTERRUPT;
		}
	}
}

TCPsocket detect_error()
{
	const threading::lock lock(*global_mutex);
	if(socket_errors > 0) {
		for(socket_state_map::iterator i = sockets_locked.begin(); i != sockets_locked.end(); ++i) {
			if(i->second == SOCKET_ERROR) {
				--socket_errors;
				const TCPsocket sock = i->first;
				sockets_locked.erase(i);
				remove_buffers(sock);
				pending_receives.erase(std::remove(pending_receives.begin(),pending_receives.end(),sock),pending_receives.end());
				return sock;
			}
		}
	}

	return 0;
}

std::pair<network::statistics,network::statistics> get_current_transfer_stats(TCPsocket sock)
{
	const threading::lock lock(*global_mutex);
	return transfer_stats[sock];
}

}

