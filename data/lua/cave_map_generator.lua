local helper = wesnoth.require "lua/helper.lua"
local MG = wesnoth.require "lua/mapgen_helper.lua"
local LS = wesnoth.require "lua/location_set.lua"
local T = helper.set_wml_tag_metatable {}
local random = wesnoth.random

local callbacks = {}

function callbacks.generate_map(params)
	local map = MG.create_map(params.map_width, params.map_height, params.terrain_wall)

	local function build_chamber(x, y, locs_set, size, jagged)
		if locs_set:get(x,y) or not map:on_board(x, y) or size == 0 then
			return
		end
		locs_set:insert(x,y)
		for xn, yn in MG.adjacent_tiles(x, y) do
			if random(100) <= 100 - jagged then
				build_chamber(xn, yn, locs_set, size - 1, jagged)
			end
		end
	end

	local function clear_tile(x, y)
		if not map:on_board(x,y) then
			return
		end
		if map:get_tile(x,y) == params.terrain_castle or map:get_tile(x,y) == params.terrain_keep then
			return
		end
		local r = random(1000)
		if r <= params.village_density then
			map:set_tile(x, y, params.terrain_village)
		else
			map:set_tile(x, y, params.terrain_clear)
		end
	end

	local chambers = {}
	local chambers_by_id = {}
	local passages = {}

	for chamber in helper.child_range(params, "chamber") do
		local chance = tonumber(chamber.chance) or 100
		local x = chamber.x
		local y = chamber.y
		local id = chamber.id
		if chance == 0 or random(100) > chance then
			goto continue
		end
		if type(x) == "string" then
			local x_min, x_max = x:match("(%d+)-(%d+)")
			x = random(tonumber(x_min), tonumber(x_max))
		end
		if type(y) == "string" then
			local y_min, y_max = y:match("(%d+)-(%d+)")
			y = random(tonumber(y_min), tonumber(y_max))
		end
		local locs_set = LS.create()
		build_chamber(x, y, locs_set, chambers.size or 3, chambers.size or 0)
		local items = {}
		for item in helper.child_range(chamber, "item_location") do
			table.insert(items, item)
		end
		table.insert(chambers, {
			center_x = x,
			center_y = y,
			side_num = chamber.side,
			locs_set = locs_set,
			id = id,
			items = items,
		})
		chambers_by_id[id] = chambers[#chambers]
		for passage in helper.child_range(chamber, "passage") do
			local dst = chambers_by_id[passage.destination]
			if dst ~= nil then
				table.insert(passages, {
					start_x = x,
					start_y = y,
					dest_x = dst.center_x,
					dest_y = dst.center_y,
					data = passage,
				})
			end
		end
		::continue::
	end

	for i,v in ipairs(chambers) do
		local locs_list = {}
		for k2,loc in ipairs(v.locs_set:to_pairs()) do
			local x, y = table.unpack(loc)
			clear_tile(x, y)
			if map:on_inner_board(x, y) then
				table.insert(locs_list, loc)
			end
		end
		for i1, item in ipairs(v.items or {}) do
			local index = random(#locs_list)
			local loc = locs_list[index]
			table.remove(locs_list, index)
			local x, y = table.unpack(loc)
		
			if item.id then
				map:add_location(x, y, item.id)
			end
		
			if item.place_castle then
				map:set_tile(x, y, params.terrain_keep)
				for x2, y2 in MG.adjacent_tiles(x, y) do
					map:set_tile(x2, y2, params.terrain_castle)
				end
			end
		end
	end

	for i,v in ipairs(passages) do
		if random(100) <= (v.data.chance or 100) then
			local windiness = v.data.windiness or 0
			local laziness = math.max(v.data.laziness or 1, 1)
			local width = math.max(v.data.width or 1, 1)
			local jagged = v.data.jagged or 0
			local calc = function(x, y, current_cost)
				local res = 1.0
				if map:get_tile(x, y) == params.terrain_wall then
					res = laziness
				end
				if windiness > 1 then
					res = res * random(windiness)
				end
				return res
			end
			local path, cost = wesnoth.find_path(v.start_x, v.start_y, v.dest_x, v.dest_y, calc, params.map_width, params.map_height)
			for i, loc in ipairs(path) do
				local locs_set = LS.create()
				build_chamber(loc[1], loc[2], locs_set, width, jagged)
				for k,v in ipairs(locs_set:to_pairs()) do
					clear_tile(v[1], v[2])
				end
			end
		end

	end

	return tostring(map)
end

return callbacks
