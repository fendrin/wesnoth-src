/* $Id$ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 *  @file
 *  Video-testprogram, standalone
 */

#include "global.hpp"

#include "font.hpp"
#include "foreach.hpp"
#include "image.hpp"
#include "log.hpp"
#include "preferences.hpp"
#include "preferences_display.hpp"
#include "sdl_utils.hpp"
#include "video.hpp"

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#include <vector>
#include <map>
#include <algorithm>

static lg::log_domain log_display("display");
#define LOG_DP LOG_STREAM(info, log_display)
#define ERR_DP LOG_STREAM(err, log_display)
#define WRN_DP LOG_STREAM(warn, log_display)

// handle extension functions
// TODO: currently only used here, but should be moves to some gl.hpp files
namespace glext
{
// Declare a pointer to the extension function and name its type
#define GLEXT_DECL(type, name) \
	typedef type name##_t;\
	name##_t name;

// functions used by FBO extension
GLEXT_DECL(PFNGLBINDFRAMEBUFFEREXTPROC, glBindFramebufferEXT);
GLEXT_DECL(PFNGLGENFRAMEBUFFERSEXTPROC, glGenFramebuffersEXT);
GLEXT_DECL(PFNGLDELETEFRAMEBUFFERSEXTPROC, glDeleteFramebuffersEXT);
GLEXT_DECL(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC, glCheckFramebufferStatusEXT);
GLEXT_DECL(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC, glFramebufferTexture2DEXT);

// Get the pointer to the extension function,
// check if existing and report its name if error
#define GLEXT_GET(name, error) \
	glext::name = (name##_t) SDL_GL_GetProcAddress(#name); \
	if(!glext::name) { \
		error += std::string(#name) + " "; \
	}

// load and check all extension functions
bool init_functions()
{
	std::string err;
	GLEXT_GET(glBindFramebufferEXT, err);
	GLEXT_GET(glGenFramebuffersEXT, err);
	GLEXT_GET(glDeleteFramebuffersEXT, err);
	GLEXT_GET(glCheckFramebufferStatusEXT, err);
	GLEXT_GET(glFramebufferTexture2DEXT, err);
	if(!err.empty()) {
		ERR_DP << "Can't find extensions functions: " << err << "\n";
		return false;
	}
	return true;
}

} //namespace glext


namespace {
	bool fullScreen = false;
	int disallow_resize = 0;
}
void resize_monitor::process(events::pump_info &info) {
	if(info.resize_dimensions.first >= preferences::min_allowed_width()
	&& info.resize_dimensions.second >= preferences::min_allowed_height()
	&& disallow_resize == 0) {
		preferences::set_resolution(info.resize_dimensions);
	}
}

resize_lock::resize_lock()
{
	++disallow_resize;
}

resize_lock::~resize_lock()
{
	--disallow_resize;
}

static unsigned int get_flags(unsigned int flags)
{
	// SDL under Windows doesn't seem to like hardware surfaces
	// for some reason.
#if !(defined(_WIN32) || defined(__APPLE__) || defined(__AMIGAOS4__))
		flags |= SDL_HWSURFACE;
#endif
	if((flags&SDL_FULLSCREEN) == 0)
		flags |= SDL_RESIZABLE;

	//OGL
	flags |= SDL_OPENGL;

	return flags;
}

namespace {
struct event {
	int x, y, w, h;
	bool in;
	event(const SDL_Rect& rect, bool i) : x(i ? rect.x : rect.x + rect.w), y(rect.y), w(rect.w), h(rect.h), in(i) { }
};
bool operator<(const event& a, const event& b) {
	if (a.x != b.x) return a.x < b.x;
	if (a.in != b.in) return a.in;
	if (a.y != b.y) return a.y < b.y;
	if (a.h != b.h) return a.h < b.h;
	if (a.w != b.w) return a.w < b.w;
	return false;
}
bool operator==(const event& a, const event& b) {
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h && a.in == b.in;
}

struct segment {
	int x, count;
	segment() : x(0), count(0) { }
	segment(int x, int count) : x(x), count(count) { }
};


std::vector<SDL_Rect> update_rects;
std::vector<event> events;
std::map<int, segment> segments;

static void calc_rects()
{
	events.clear();

	foreach (SDL_Rect const &rect, update_rects) {
		events.push_back(event(rect, true));
		events.push_back(event(rect, false));
	}

	std::sort(events.begin(), events.end());
	std::vector<event>::iterator events_end = std::unique(events.begin(), events.end());

	segments.clear();
	update_rects.clear();

	for (std::vector<event>::iterator iter = events.begin(); iter != events_end; ++iter) {
		std::map<int, segment>::iterator lower = segments.find(iter->y);
		if (lower == segments.end()) {
			lower = segments.insert(std::make_pair(iter->y, segment())).first;
			if (lower != segments.begin()) {
				std::map<int, segment>::iterator prev = lower;
				--prev;
				lower->second = prev->second;
			}
		}

		if (lower->second.count == 0) {
			lower->second.x = iter->x;
		}

		std::map<int, segment>::iterator upper = segments.find(iter->y + iter->h);
		if (upper == segments.end()) {
			upper = segments.insert(std::make_pair(iter->y + iter->h, segment())).first;
			std::map<int, segment>::iterator prev = upper;
			--prev;
			upper->second = prev->second;
		}

		if (iter->in) {
			while (lower != upper) {
				++lower->second.count;
				++lower;
			}
		} else {
			while (lower != upper) {
				lower->second.count--;
				if (lower->second.count == 0) {
					std::map<int, segment>::iterator next = lower;
					++next;

					int x = lower->second.x, y = lower->first;
					unsigned w = iter->x - x;
					unsigned h = next->first - y;
					SDL_Rect a = create_rect(x, y, w, h);

					if (update_rects.empty()) {
						update_rects.push_back(a);
					} else {
						SDL_Rect& p = update_rects.back(), n;
						int pa = p.w * p.h, aa = w * h, s = pa + aa;
						int thresh = 51;

						n.w = std::max<int>(x + w, p.x + p.w);
						n.x = std::min<int>(p.x, x);
						n.w -= n.x;
						n.h = std::max<int>(y + h, p.y + p.h);
						n.y = std::min<int>(p.y, y);
						n.h -= n.y;

						if (s * 100 < thresh * n.w * n.h) {
							update_rects.push_back(a);
						} else {
							p = n;
						}
					}

					if (lower == segments.begin()) {
						segments.erase(lower);
					} else {
						std::map<int, segment>::iterator prev = lower;
						--prev;
						if (prev->second.count == 0) segments.erase(lower);
					}

					lower = next;
				} else {
					++lower;
				}
			}
		}
	}
}


bool update_all = false;
}

static void clear_updates()
{
	update_all = false;
	update_rects.clear();
}

namespace {

surface frameBuffer = NULL;
bool fake_interactive = false;
}

bool non_interactive()
{
	if (fake_interactive)
		return false;
	return SDL_GetVideoSurface() == NULL;
}

surface display_format_alpha(surface surf)
{
	if(SDL_GetVideoSurface() != NULL)
		return SDL_DisplayFormatAlpha(surf);
	else if(frameBuffer != NULL)
		return SDL_ConvertSurface(surf,frameBuffer->format,0);
	else
		return NULL;
}

surface get_video_surface()
{
	return frameBuffer;
}

SDL_Rect screen_area()
{
	return create_rect(0, 0, frameBuffer->w, frameBuffer->h);
}

void update_rect(size_t x, size_t y, size_t w, size_t h)
{
	update_rect(create_rect(x, y, w, h));
}

void update_rect(const SDL_Rect& rect_value)
{
	if(update_all)
		return;

	SDL_Rect rect = rect_value;

	surface const fb = SDL_GetVideoSurface();
	if(fb != NULL) {
		if(rect.x < 0) {
			if(rect.x*-1 >= int(rect.w))
				return;

			rect.w += rect.x;
			rect.x = 0;
		}

		if(rect.y < 0) {
			if(rect.y*-1 >= int(rect.h))
				return;

			rect.h += rect.y;
			rect.y = 0;
		}

		if(rect.x + rect.w > fb->w) {
			rect.w = fb->w - rect.x;
		}

		if(rect.y + rect.h > fb->h) {
			rect.h = fb->h - rect.y;
		}

		if(rect.x >= fb->w) {
			return;
		}

		if(rect.y >= fb->h) {
			return;
		}
	}

	update_rects.push_back(rect);
}

void update_whole_screen()
{
	update_all = true;
}

fbo::~fbo()
{
	glDeleteTextures(1, &fbo_id_);
	glext::glDeleteFramebuffersEXT(1, &tex_id_);
}

bool fbo::init(unsigned w, unsigned h, unsigned attach)
{
	attach_ = attach;

	GLint max_attach = 0;
	glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_attach);
	if(attach_ >= max_attach) {
		ERR_DP << "Not enough FBO colour attachment points.\n";
		return false;
	}

	if(fbo_id_ == 0)
		glext::glGenFramebuffersEXT(1, &fbo_id_);

	if(fbo_id_ == 0) {
		ERR_DP << "Can't create new FBO.\n";
		return false;
	}

	glext::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_id_);
	glViewport(0, 0, w, h);

	// Create texture for the FBO and attach it
	if(tex_id_ == 0)
		glGenTextures(1, &tex_id_);
	if(tex_id_ == 0) {
		ERR_DP << "Can't create new texture for FBO.\n";
		return false;
	}
	glBindTexture(GL_TEXTURE_2D, tex_id_);

	// Disable mipmaps since we don't generate them
	// and some drivers will not consider FBO 'complete' with such incomplete texture
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glTexImage2D(GL_TEXTURE_2D, 0 /*level*/, GL_RGBA8, frameBuffer->w, frameBuffer->h,
			0 /*border*/, GL_BGRA, GL_UNSIGNED_BYTE, NULL /*data*/);
	glext::glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + attach_, GL_TEXTURE_2D, tex_id_, 0);

	// Check FBO status
	GLenum fbo_status = glext::glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	if(fbo_status == GL_FRAMEBUFFER_COMPLETE_EXT)
			return true; //ok, we are done

	// status error, report it
	#define FBO_ERROR_CASE(val) case val: ERR_DP << #val << "\n"; break;
	switch(fbo_status) {
		FBO_ERROR_CASE(GL_FRAMEBUFFER_UNSUPPORTED_EXT)
		FBO_ERROR_CASE(GL_FRAMEBUFFER_UNDEFINED)
		FBO_ERROR_CASE(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
		FBO_ERROR_CASE(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
		FBO_ERROR_CASE(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER)
		FBO_ERROR_CASE(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
		FBO_ERROR_CASE(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER)
		default:
			ERR_DP << "Unknown FBO error\n";
			break;
	}
	#undef FBO_ERROR_CASE
	return false;
}

void fbo::enable()
{
	glext::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_id_);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + attach_);
	glReadBuffer(GL_COLOR_ATTACHMENT0_EXT + attach_);
}

void fbo::disable()
{
	glext::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glDrawBuffer(GL_BACK);
	glReadBuffer(GL_BACK);
}

void fbo::bind_texture() {
	glBindTexture(GL_TEXTURE_2D, tex_id_);
}

CVideo::CVideo(FAKE_TYPES type) : mode_changed_(false), bpp_(0), fake_screen_(false), help_string_(0), updatesLocked_(0), fbo_()
{
	initSDL();
	switch(type)
	{
		case NO_FAKE:
			break;
		case FAKE:
			make_fake();
			break;
		case FAKE_TEST:
			make_test_fake();
			break;
	}
}

void CVideo::initSDL()
{
	const int res = SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);

	if(res < 0) {
		ERR_DP << "Could not initialize SDL_video: " << SDL_GetError() << "\n";
		throw CVideo::error();
	}
}

CVideo::~CVideo()
{
	LOG_DP << "calling SDL_Quit()\n";
	SDL_Quit();
	LOG_DP << "called SDL_Quit()\n";
}

void CVideo::blit_surface(int x, int y, surface surf, SDL_Rect* srcrect, SDL_Rect* clip_rect)
{
	surface target(getSurface());
	SDL_Rect dst = create_rect(x, y, 0, 0);

	const clip_rect_setter clip_setter(target, clip_rect, clip_rect != NULL);
	sdl_blit(surf,srcrect,target,&dst);
}

void CVideo::make_fake()
{
	fake_screen_ = true;
	frameBuffer = SDL_CreateRGBSurface(SDL_SWSURFACE,16,16,24,0xFF0000,0xFF00,0xFF,0);
	image::set_pixel_format(frameBuffer->format);
}

void CVideo::make_test_fake(const unsigned width,
			const unsigned height, const unsigned bpp)
{
	frameBuffer = SDL_CreateRGBSurface(SDL_SWSURFACE,
			width, height, bpp, 0xFF0000, 0xFF00, 0xFF, 0);
	image::set_pixel_format(frameBuffer->format);

	fake_interactive = true;

}

int CVideo::modePossible( int x, int y, int bits_per_pixel, int flags, bool current_screen_optimal )
{

	int bpp = SDL_VideoModeOK( x, y, bits_per_pixel, get_flags(flags) );
	if(current_screen_optimal)
	{
		const SDL_VideoInfo* const video_info = SDL_GetVideoInfo();
		/* if current video_info is smaller than the mode checking and the checked mode is supported
		(meaning that probably the video card supports higher resolutions than the monitor)
		that means that we just need to adjust the resolution and the bpp is ok
		*/
		if(bpp==0 && video_info->current_h<y && video_info->current_w<x){
			return bits_per_pixel;
		}
	}
	return bpp;
}

int CVideo::setMode( int x, int y, int bits_per_pixel, int flags )
{
	update_rects.clear();
	if (fake_screen_) return 0;
	mode_changed_ = true;

	flags = get_flags(flags);
	const int res = SDL_VideoModeOK( x, y, bits_per_pixel, flags );

	if( res == 0 )
		return 0;

	fullScreen = (flags & FULL_SCREEN) != 0;

	//Be sure to use double buffering
	const int double_buffer = 1;
	//Disbale V-sync for the moment
	//TODO enable this later, update error message below
	//const int vsync = 0;

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, double_buffer);
	//SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, vsync);

	frameBuffer = SDL_SetVideoMode( x, y, bits_per_pixel, flags );

	if(frameBuffer == NULL) {
		ERR_DP << "Can't set video mode.\n";
		throw CVideo::error();
	}

	std::cout << "OpenGL: " << glGetString(GL_VERSION) << "\n";
	std::cout << "GPU: " << glGetString(GL_RENDERER) << " by "<< glGetString(GL_VENDOR) << "\n";

	//TODO move extension checking in proper function
	std::ostringstream ext;
	ext << glGetString(GL_EXTENSIONS);
	std::string extensions = ext.str();
	if(extensions.find("GL_EXT_framebuffer_object",0) == std::string::npos) {
		ERR_DP << "Driver don't support GL_EXT_framebuffer_object.\n";
		throw CVideo::error();
	}

	//We have OpenGL context now, load extension functions
	if(glext::init_functions() == false) {
		ERR_DP << "Missing OpenGL extension functions.\n";
		throw CVideo::error();
	}

	int val = 0;

	if(SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &val)) {
		WRN_DP << "Can't verify double-buffering status.\n";
	} else if(val != double_buffer) {
		ERR_DP << "Can't set double-buffering.\n";
		throw CVideo::error();
	}

// 	if(SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL, &val)) {
// 		WRN_DP << "Can't verify V-Sync status.\n";
// 	} else if (val != vsync) {
// 		ERR_DP << "Can't disable V-sync.\n";
// 	}

	//the clip rectangle of frame buffer is not always reset when using OpengGL
	SDL_SetClipRect(SDL_GetVideoSurface(), NULL);

	glViewport(0, 0, frameBuffer->w, frameBuffer->h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// we reverse Y coordinates to imitate SDL
	glOrtho(0, frameBuffer->w, frameBuffer->h, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Now setting up the Frame Buffer Object used for rendering
	// (can be already there when doing resolution change)
	if(fbo_.init(frameBuffer->w, frameBuffer->h, 0) == false){
		ERR_DP << "Can't initialize FBO.\n";
		throw CVideo::error();
	};

	// We will always read and write only on the FBO
	fbo_.enable();

	image::set_pixel_format(frameBuffer->format);

	return frameBuffer->format->BitsPerPixel;
}

bool CVideo::modeChanged()
{
	bool ret = mode_changed_;
	mode_changed_ = false;
	return ret;
}

int CVideo::getx() const
{
	return frameBuffer->w;
}

int CVideo::gety() const
{
	return frameBuffer->h;
}

void CVideo::flip()
{
	if(fake_screen_)
		return;

	// Stop rendering on FBO, and now use back buffer
	fbo_.disable();

	// Render the texture attached to the FBO on the back buffer
	fbo_.bind_texture();

	if(update_all) {
		sdl_flip(frameBuffer);
	} else if(!update_rects.empty()) {
		calc_rects();
		if(!update_rects.empty()) {
			sdl_update_rects(frameBuffer, update_rects.size(), &update_rects[0]);
		}
	}
	// Back buffer is updated now, we can swap
	SDL_GL_SwapBuffers();

	//restore normal rendering on FBO
	fbo_.enable();

	clear_updates();

	//NOTE: update_all seems to prevent black blinking in fullscreen for GUI
	update_all = true;
}

void CVideo::lock_updates(bool value)
{
	if(value == true)
		++updatesLocked_;
	else
		--updatesLocked_;
}

bool CVideo::update_locked() const
{
	return updatesLocked_ > 0;
}

surface& CVideo::getSurface()
{
	return frameBuffer;
}

bool CVideo::isFullScreen() const { return fullScreen; }

void CVideo::setBpp( int bpp )
{
	bpp_ = bpp;
}

int CVideo::getBpp()
{
	return bpp_;
}

int CVideo::set_help_string(const std::string& str)
{
	font::remove_floating_label(help_string_);

	const SDL_Color color = { 0, 0, 0, 0xbb };

#ifdef USE_TINY_GUI
	int size = font::SIZE_NORMAL;
#else
	int size = font::SIZE_LARGE;
#endif

	while(size > 0) {
		if(font::line_width(str, size) > getx()) {
			size--;
		} else {
			break;
		}
	}

#ifdef USE_TINY_GUI
	const int border = 2;
#else
	const int border = 5;
#endif

	font::floating_label flabel(str);
	flabel.set_font_size(size);
	flabel.set_position(getx()/2, gety());
	flabel.set_bg_color(color);
	flabel.set_border_size(border);

	help_string_ = font::add_floating_label(flabel);

	const SDL_Rect& rect = font::get_floating_label_rect(help_string_);
	font::move_floating_label(help_string_,0.0,-double(rect.h));
	return help_string_;
}

void CVideo::clear_help_string(int handle)
{
	if(handle == help_string_) {
		font::remove_floating_label(handle);
		help_string_ = 0;
	}
}

void CVideo::clear_all_help_strings()
{
	clear_help_string(help_string_);
}
