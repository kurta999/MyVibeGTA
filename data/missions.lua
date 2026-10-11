-- Mission definitions and all progression rules. Indices here are one-based.
-- The host supplies a read-only snapshot, and applies returned transitions.
local missions = {
  {id="sunset-circuit", name="Sunset Circuit", kind="drive", start={x=330,z=250}, seconds=150, reward=600, prerequisite=0, goals={{x=750, z=250, requires="car", objective="Drive a car through the checkpoints"}, {x=750, z=640, requires="car", objective="Drive a car through the checkpoints"}, {x=1200, z=640, requires="car", objective="Drive a car through the checkpoints"}, {x=1650, z=1030, requires="car", objective="Drive a car through the checkpoints"}}},
  {id="beach-cache", name="Beach Cache", kind="collect", start={x=750,z=1450}, seconds=180, reward=450, prerequisite=1, goals={{x=520, z=1710, requires="foot", objective="Collect the beach caches on foot"}, {x=1150, z=1770, requires="foot", objective="Collect the beach caches on foot"}, {x=1900, z=1730, requires="foot", objective="Collect the beach caches on foot"}}},
  {id="harbor-run", name="Harbor Run", kind="boat", start={x=300,z=1710}, seconds=170, reward=700, prerequisite=2, goals={{x=300, z=1930, requires="boat", objective="Pilot a boat through the harbor"}, {x=1000, z=2010, requires="boat", objective="Pilot a boat through the harbor"}, {x=1850, z=1930, requires="boat", objective="Pilot a boat through the harbor"}}},
  {id="boardwalk-range", name="Boardwalk Range", kind="targets", start={x=1200,z=1030}, seconds=120, reward=350, prerequisite=3, goals={{x=1300, z=1030, requires="target", objective="Shoot the orange practice targets"}, {x=1450, z=1030, requires="target", objective="Shoot the orange practice targets"}, {x=1600, z=1030, requires="target", objective="Shoot the orange practice targets"}}},
  {id="neon-sprint", name="Neon Sprint", kind="bike", start={x=340,z=350}, seconds=110, reward=550, prerequisite=4, goals={{x=300, z=640, requires="bike", objective="Ride a motorcycle through the checkpoints"}, {x=750, z=640, requires="bike", objective="Ride a motorcycle through the checkpoints"}, {x=1200, z=1030, requires="bike", objective="Ride a motorcycle through the checkpoints"}, {x=1650, z=1420, requires="bike", objective="Ride a motorcycle through the checkpoints"}}},
  {id="coastline-finale", name="Coastline Finale", kind="finale", start={x=1650,z=1420}, seconds=330, reward=1500, prerequisite=5, goals={{x=1600, z=1520, requires="car", objective="Drive a car to the beach approach"}, {x=1800, z=1730, requires="target", objective="Exit and shoot the orange beach target"}, {x=1490, z=1960, requires="boat", objective="Pilot a boat to the harbor marker"}}},
  {id="country-snow-delivery", name="Country to Snow Delivery", kind="drive", start={x=4200,z=4000}, seconds=390, reward=950, prerequisite=0, goals={{x=5800, z=4000, requires="car", objective="Drive a car through the checkpoints"}, {x=5800, z=8500, requires="car", objective="Drive a car through the checkpoints"}, {x=5800, z=14000, requires="car", objective="Drive a car through the checkpoints"}, {x=4000, z=14000, requires="car", objective="Drive a car through the checkpoints"}}},
  {id="snow-crossworld-run", name="Snowfield Range", kind="targets", start={x=4000,z=14000}, seconds=180, reward=1250, prerequisite=0, goals={{x=3800, z=14000, requires="target", objective="Shoot the orange practice targets"}, {x=3600, z=14000, requires="target", objective="Shoot the orange practice targets"}, {x=3400, z=14000, requires="target", objective="Shoot the orange practice targets"}, {x=3200, z=14000, requires="target", objective="Shoot the orange practice targets"}}},
  {id="desert-supply-route", name="Desert Caches", kind="collect", start={x=12000,z=3000}, seconds=180, reward=1050, prerequisite=0, goals={{x=12090, z=3000, requires="foot", objective="Collect the beach caches on foot"}, {x=12240, z=3000, requires="foot", objective="Collect the beach caches on foot"}, {x=12420, z=3000, requires="foot", objective="Collect the beach caches on foot"}, {x=12600, z=3000, requires="foot", objective="Collect the beach caches on foot"}}},
  {id="savanna-relay", name="Savanna Relay", kind="drive", start={x=12000,z=14000}, seconds=430, reward=1100, prerequisite=0, goals={{x=14500, z=14000, requires="car", objective="Drive a car through the checkpoints"}, {x=14500, z=8500, requires="car", objective="Drive a car through the checkpoints"}, {x=12000, z=8500, requires="car", objective="Drive a car through the checkpoints"}, {x=10700, z=8500, requires="car", objective="Drive a car through the checkpoints"}}}
}
local function distance(c, p)
    return math.sqrt((c.x-p.x)^2 + (c.z-p.z)^2)
end
local function goal(c)
    local m = missions[c.active]
    return m and m.goals[c.step]
end
local function objective(c)
    local g = goal(c)
    return g and g.objective or "Follow the gold line to the next mission."
end
local function transition(c, event, message, banner, detail)
    return {active=c.active, step=c.step, time=math.max(0,c.time),
        event=event or "none", message=message or "-", banner=banner or "-", detail=detail or "-"}
end
local function start_candidate(c)
    if c.active ~= 0 or c.health <= 0 then return 0 end
    for i,m in ipairs(missions) do
        if not c.done[i] and distance(c,m.start) < 75 then return i end
    end
    return 0
end
local function start(c)
    local i = start_candidate(c)
    if i == 0 then return transition(c) end
    local m = missions[i]
    if m.prerequisite > 0 and not c.done[m.prerequisite] then
        return transition(c,"locked","Finish "..missions[m.prerequisite].name.." to unlock this mission.")
    end
    c.active, c.step, c.time = i, 1, m.seconds
    return transition(c,"start","MISSION STARTED: "..m.name,m.name,objective(c))
end
local function update(c, dt, event)
    if c.active == 0 then return transition(c) end
    c.time = c.time - dt
    if c.time <= 0 or c.health <= 0 then
        local reason = c.health <= 0 and "You were defeated" or "Time ran out"
        c.active,c.step = 0,1
        return transition(c,"fail","MISSION FAILED. Return to a marker to retry.","MISSION FAILED",reason)
    end
    local g=goal(c)
    local hit=g and g.requires=="target" and event=="target_hit"
    local reached=g and g.requires~="target" and g.requires==c.vehicle and distance(c,g)<45
    if hit or reached then
        c.step=c.step+1
        if c.step>#missions[c.active].goals then return transition(c,"complete") end
        return transition(c,hit and "hit" or "checkpoint",objective(c),
            hit and "TARGET HIT" or "CHECKPOINT REACHED",objective(c))
    end
    return transition(c)
end
return {version=1,missions=missions,start=start,start_candidate=start_candidate,update=update,objective=objective,
    is_target=function(c) local g=goal(c);return g and g.requires=="target" or false end,
    next=function(c) for i,_ in ipairs(missions) do if not c.done[i] then return i end end return 0 end}
