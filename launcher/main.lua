local settings = {
  use_bloom = 1,
  trilinear_filter = 1,
  disable_mipmaps = 0,
  language = 0,
  crouch_toggle = 1,
  character_shadows = 1,
  drop_highest_lod = 0,
  show_weapon_menu = 0,
  vsync_enabled = 1,
  decal_limit = 0.5,
  debris_limit = 1.0,
  mod_file = "",
  force_widescreen = 0,
  stick_deadzone = 0.1,
  aspect_ratio_x_mult = 1.18,
  aspect_ratio_y_mult = 0.84,
}

local meta = {
  use_bloom =        {type="int", min=0, max=1, step=1, label="Bloom"},
  trilinear_filter = {type="int", min=0, max=1, step=1, label="Trilinear Filter"},
  disable_mipmaps =  {type="int", min=0, max=1, step=1, label="Disable Mipmaps"},
  language =         {type="int", min=0, max=6, step=1, label="Language"},
  crouch_toggle =    {type="int", min=0, max=1, step=1, label="Crouch Toggle"},
  character_shadows={type="int", min=0, max=2, step=1, label="Character Shadows", hint="1=blob, 2=foot"},
  drop_highest_lod = {type="int", min=0, max=1, step=1, label="Drop Highest LOD"},
  show_weapon_menu = {type="int", min=0, max=1, step=1, label="Show Weapon Menu"},
  vsync_enabled =    {type="int", min=0, max=1, step=1, label="VSync"},
  decal_limit =      {type="float", min=0.0, max=1.0, step=0.01, label="Decal Limit"},
  debris_limit =     {type="float", min=0.0, max=3.0, step=0.01, label="Debris Limit"},
  mod_file =         {type="string", maxlen=255, label="Mod File"},
  force_widescreen = {type="int", min=0, max=1, step=1, label="Force Widescreen"},
  stick_deadzone =   {type="float", min=0.0, max=1.0, step=0.01, label="Stick Deadzone"},
  aspect_ratio_x_mult = {type="float", min=0.5, max=2.0, step=0.01, label="Aspect X Mult"},
  aspect_ratio_y_mult = {type="float", min=0.5, max=2.0, step=0.01, label="Aspect Y Mult"},
}

local order = {
  "use_bloom",
  "trilinear_filter",
  "disable_mipmaps",
  "language",
  "character_shadows",
  "drop_highest_lod",
  "vsync_enabled",
  "decal_limit",
  "debris_limit",
  "force_widescreen",
  "stick_deadzone",
  "aspect_ratio_x_mult",
  "aspect_ratio_y_mult",
}

local languageNames = {
  [0] = "English",
  [1] = "French", 
  [2] = "Spanish",
  [3] = "Italian",
  [4] = "Russian",
  [5] = "Japanese",
  [6] = "German"
}

local FILE_PATH = "conf/config.txt"
local FONT
local BACKGROUND_IMAGE
local ui = {sel=1, edit=false, message="", message_t=0, mode="launcher"}
local axisRepeat = {x=0, y=0, t=0, repeatDelay=0.35, repeatRate=0.08}

local keyRepeat = {
  left = {held = false, timer = 0},
  right = {held = false, timer = 0},
  up = {held = false, timer = 0},
  down = {held = false, timer = 0},
  repeatDelay = 0.35,
  repeatRate = 0.08
}

local gamepadRepeat = {
  dpleft = {held = false, timer = 0},
  dpright = {held = false, timer = 0},
  dpup = {held = false, timer = 0},
  dpdown = {held = false, timer = 0},
  repeatDelay = 0.35,
  repeatRate = 0.08
}

local launcherOptions = {"Play", "Settings"}

local function launchGame()
  love.event.quit(0)
end

local function clamp(v, a, b) return math.max(a, math.min(b, v)) end

local function roundToStep(v, step)
  return math.floor((v/step) + 0.5) * step
end

local MSFParser = require("msf_parser")

local menuSounds = {
  move = 915,     -- menu/move.mp3
  select = 916,   -- menu/select.mp3
  slider = 917    -- menu/slidermove.mp3
}
local backgroundMusicIndex = 1359  -- wavs/max_theme_short.mp3
local backgroundMusic = nil

local menuAudioSources = {}
local sharedArchive = nil

local function loadSharedArchive()
  if not sharedArchive then
    local error
    sharedArchive, error = MSFParser.loadArchive()
  end
  return sharedArchive
end

local duckingActive = false
local duckingTimer = 0
local normalVolume = 0.5
local duckedVolume = 0.2
local duckDuration = 0.8
local fadeSpeed = 2.0

local function updateDucking(dt)
  if backgroundMusic then
    local targetVolume = duckingActive and duckedVolume or normalVolume
    local currentVolume = backgroundMusic:getVolume()
    
    if math.abs(currentVolume - targetVolume) > 0.01 then
      local newVolume = currentVolume + (targetVolume - currentVolume) * fadeSpeed * dt
      backgroundMusic:setVolume(newVolume)
    end
    
    if duckingActive then
      duckingTimer = duckingTimer - dt
      if duckingTimer <= 0 then
        duckingActive = false
      end
    end
  end
end

local function duckBackgroundMusic()
  duckingActive = true
  duckingTimer = duckDuration
end

local function preloadMenuSounds()
  local archive = loadSharedArchive()
  if not archive then return end
  
  for soundType, fileIndex in pairs(menuSounds) do
    local audioSource, tempPath = MSFParser.loadAudioSource(fileIndex, "static", archive)
    if audioSource then
      menuAudioSources[soundType] = audioSource
    end
    if tempPath then
      love.filesystem.remove(tempPath)
    end
  end
end

local function playMenuSound(soundType)
  local audioSource = menuAudioSources[soundType]
  if audioSource then
    duckBackgroundMusic()
    audioSource:stop()
    audioSource:play()
  end
end


local function startBackgroundMusic()
  if not backgroundMusic or not backgroundMusic:isPlaying() then
    local archive = loadSharedArchive()
    local tempFile = MSFParser.extractFileSimple(backgroundMusicIndex, archive)
    if tempFile then
      backgroundMusic = love.audio.newSource(tempFile, "stream")
      backgroundMusic:setLooping(true)
      backgroundMusic:setVolume(normalVolume)
      backgroundMusic:play()
    end
  end
end

local function stopBackgroundMusic()
  if backgroundMusic then
    backgroundMusic:stop()
    backgroundMusic = nil
  end
end

local function parseConfigText(text)
  for line in (text or ""):gmatch("[^\r\n]+") do
    local s = line:gsub("^%s+",""):gsub("%s+$","")
    if s ~= "" and not s:match("^//") then
      s = s:gsub("//.*$", "")
      local key, val = s:match("^(%S+)%s+(.+)$")
      if key and val and settings[key] ~= nil then
        if meta[key].type == "int" then
          settings[key] = tonumber(val) or settings[key]
        elseif meta[key].type == "float" then
          settings[key] = tonumber(val) or settings[key]
        elseif meta[key].type == "string" then
          val = val:gsub('^"(.*)"$', '%1')
          settings[key] = val
        end
      end
    end
  end
end

local function serialize()
  local out = {}
  local function push(k, v, hint)
    if type(v) == "string" and v:find("%s") then v = '"'..v..'"' end
    table.insert(out, string.format("%s %s%s", k, tostring(v), hint and (" // "..hint) or ""))
  end
  push("use_bloom", settings.use_bloom)
  push("trilinear_filter", settings.trilinear_filter)
  push("disable_mipmaps", settings.disable_mipmaps)
  push("language", settings.language)
  push("crouch_toggle", settings.crouch_toggle)
  push("character_shadows", settings.character_shadows, "1 - one blob; 2 - foot shadows")
  push("drop_highest_lod", settings.drop_highest_lod)
  push("show_weapon_menu", settings.show_weapon_menu)
  push("vsync_enabled", settings.vsync_enabled, "Enable VSync (1=on,0=off)")
  push("decal_limit", settings.decal_limit)
  push("debris_limit", settings.debris_limit)
  if settings.mod_file ~= "" then push("mod_file", settings.mod_file) end
  push("force_widescreen", settings.force_widescreen, "0=disabled,1=enabled")
  push("stick_deadzone", settings.stick_deadzone, "0.0-1.0")
  push("aspect_ratio_x_mult", settings.aspect_ratio_x_mult)
  push("aspect_ratio_y_mult", settings.aspect_ratio_y_mult)
  return table.concat(out, "\n") .. "\n"
end


local function loadFile()
  local file = io.open(FILE_PATH, "r")
  if file then
    local data = file:read("*a")
    file:close()
    parseConfigText(data)
    ui.message, ui.message_t = "Loaded config.txt", 1.5
  else
    local file = io.open(FILE_PATH, "w")
    if file then
      file:write(serialize())
      file:close()
      ui.message, ui.message_t = "Created config.txt with defaults", 2.0
    else
      ui.message, ui.message_t = "Failed to create config.txt", 2.0
    end
  end
end

local function saveFile()
  local file = io.open(FILE_PATH, "w")
  if file then
    file:write(serialize())
    file:close()
    ui.message, ui.message_t = "Saved", 1.5
  else
    ui.message, ui.message_t = "Failed to save config.txt", 2.0
  end
end

local function moveSel(delta)
  if ui.mode == "launcher" then
    ui.sel = clamp(ui.sel + delta, 1, #launcherOptions)
  elseif ui.mode == "config" then
    ui.sel = clamp(ui.sel + delta, 1, #order)
  end
  playMenuSound("move")
end

local function adjustValue(key, dir)
  local m = meta[key]
  if not m then return end
  if m.type == "int" then
    settings[key] = clamp(settings[key] + (dir * m.step), m.min, m.max)
    playMenuSound("slider")
  elseif m.type == "float" then
    local v = settings[key] + (dir * m.step)
    v = clamp(v, m.min, m.max)
    settings[key] = tonumber(string.format("%.3f", v))
    playMenuSound("slider")
  elseif m.type == "string" then
    ui.edit = true
  end
end

local function toggleValue(key)
  local m = meta[key]
  if m and m.type == "int" and m.min == 0 and m.max == 1 then
    settings[key] = 1 - settings[key]
  end
end

local function handleAxis(dt)
  axisRepeat.t = axisRepeat.t + dt
  local joysticks = love.joystick.getJoysticks()
  local j = joysticks[1]
  if not j then return end
  local lx = j:getGamepadAxis("leftx") or 0
  local ly = j:getGamepadAxis("lefty") or 0
  local dead = 0.35
  local function repeatLogic(axis, val, onNeg, onPos)
    if math.abs(val) < dead then
      axisRepeat[axis] = 0
      return
    end
    local dir = (val > 0) and 1 or -1
    if axisRepeat[axis] == 0 then
      axisRepeat[axis] = axisRepeat.t + axisRepeat.repeatDelay
      if dir < 0 then onNeg() else onPos() end
    elseif axisRepeat.t >= axisRepeat[axis] then
      axisRepeat[axis] = axisRepeat.t + axisRepeat.repeatRate
      if dir < 0 then onNeg() else onPos() end
    end
  end
  if ui.mode == "config" then
    repeatLogic('x', lx, function() adjustValue(order[ui.sel], -1) end, function() adjustValue(order[ui.sel], 1) end)
  end
  repeatLogic('y', ly, function() moveSel(-1) end, function() moveSel(1) end)
end

local function handleKeyRepeat(dt)
  for keyName, keyData in pairs(keyRepeat) do
    if keyName ~= "repeatDelay" and keyName ~= "repeatRate" and keyData.held then
      keyData.timer = keyData.timer + dt
      if keyData.timer >= keyRepeat.repeatDelay then
        keyData.timer = keyData.timer - keyRepeat.repeatRate
        
        if keyName == "left" and ui.mode == "config" then
          adjustValue(order[ui.sel], -1)
        elseif keyName == "right" and ui.mode == "config" then
          adjustValue(order[ui.sel], 1)
        elseif keyName == "up" then
          moveSel(-1)
        elseif keyName == "down" then
          moveSel(1)
        end
      end
    end
  end
end

local function handleGamepadRepeat(dt)
  for buttonName, buttonData in pairs(gamepadRepeat) do
    if buttonName ~= "repeatDelay" and buttonName ~= "repeatRate" and buttonData.held then
      buttonData.timer = buttonData.timer + dt
      if buttonData.timer >= gamepadRepeat.repeatDelay then
        buttonData.timer = buttonData.timer - gamepadRepeat.repeatRate
        
        if buttonName == "dpleft" and ui.mode == "config" then
          adjustValue(order[ui.sel], -1)
        elseif buttonName == "dpright" and ui.mode == "config" then
          adjustValue(order[ui.sel], 1)
        elseif buttonName == "dpup" then
          moveSel(-1)
        elseif buttonName == "dpdown" then
          moveSel(1)
        end
      end
    end
  end
end

function love.load()
  love.window.setTitle("Max Payne Launcher")
  love.window.setMode(640, 480, {resizable=true, vsync=1})
  FONT = love.graphics.newFont(14)
  love.graphics.setFont(FONT)
  
  if love.filesystem.getInfo("bg.jpg") then
    BACKGROUND_IMAGE = love.graphics.newImage("bg.jpg")
  end
  
  loadFile()
  preloadMenuSounds()
  startBackgroundMusic()
end

function love.quit()
  stopBackgroundMusic()
end

function love.update(dt)
  handleAxis(dt)
  handleKeyRepeat(dt)
  handleGamepadRepeat(dt)
  updateDucking(dt)
  if ui.message_t > 0 then ui.message_t = math.max(0, ui.message_t - dt) end
end

function love.gamepadpressed(joy, button)
  if button == "dpup" or button == "dpdown" or button == "dpleft" or button == "dpright" then
    if gamepadRepeat[button] then
      gamepadRepeat[button].held = true
      gamepadRepeat[button].timer = 0
    end
  end

  if ui.mode == "launcher" then
    if button == "dpup" then moveSel(-1)
    elseif button == "dpdown" then moveSel(1)
    elseif button == "a" then
      playMenuSound("select")
      if ui.sel == 1 then
        launchGame()
      elseif ui.sel == 2 then
        ui.mode = "config"
        ui.sel = 1
      end
    elseif button == "b" or button == "back" then love.event.quit()
    end
    return
  end
  
  if ui.edit then
    if button == "dpup" then kbMove(0,-1)
    elseif button == "dpdown" then kbMove(0,1)
    elseif button == "dpleft" then kbMove(-1,0)
    elseif button == "dpright" then kbMove(1,0)
    elseif button == "a" then kbPress()
    elseif button == "x" then
      local s = settings.mod_file
      settings.mod_file = s and s:sub(1, #s-1) or ""
    elseif button == "y" then settings.mod_file = ""
    elseif button == "b" or button == "back" then ui.edit=false
    elseif button == "start" then ui.edit=false end
    return
  end

  if button == "dpup" then moveSel(-1)
  elseif button == "dpdown" then moveSel(1)
  elseif button == "dpleft" then adjustValue(order[ui.sel], -1)
  elseif button == "dpright" then adjustValue(order[ui.sel], 1)
  elseif button == "a" then 
    playMenuSound("select")
    toggleValue(order[ui.sel])
  elseif button == "x" then
    local k = order[ui.sel]
    local m = meta[k]
    if m.type == "int" then settings[k] = clamp(settings[k] or 0, m.min, m.max)
    elseif m.type == "float" then settings[k] = clamp(roundToStep(settings[k] or 0, m.step), m.min, m.max)
    elseif m.type == "string" then ui.edit = true end
  elseif button == "y" then loadFile()
  elseif button == "start" then saveFile()
  elseif button == "back" or button == "b" then 
    saveFile()
    ui.mode = "launcher"
    ui.sel = 1
  end
end

function love.gamepadreleased(joy, button)
  if button == "dpup" or button == "dpdown" or button == "dpleft" or button == "dpright" then
    if gamepadRepeat[button] then
      gamepadRepeat[button].held = false
      gamepadRepeat[button].timer = 0
    end
  end
end

function love.keypressed(key)
  if key == "up" or key == "down" or key == "left" or key == "right" then
    if keyRepeat[key] then
      keyRepeat[key].held = true
      keyRepeat[key].timer = 0
    end
  end

  if ui.mode == "launcher" then
    if key == "up" then moveSel(-1)
    elseif key == "down" then moveSel(1)
    elseif key == "return" or key == "space" then
      playMenuSound("select")
      if ui.sel == 1 then
        launchGame()
      elseif ui.sel == 2 then
        ui.mode = "config"
        ui.sel = 1
      end
    elseif key == "escape" then love.event.quit()
    end
    return
  end
  
  if key == "up" then moveSel(-1)
  elseif key == "down" then moveSel(1)
  elseif key == "left" then adjustValue(order[ui.sel], -1)
  elseif key == "right" then adjustValue(order[ui.sel], 1)
  elseif key == "return" or key == "space" then 
    playMenuSound("select")
    toggleValue(order[ui.sel])
  elseif key == "escape" then 
    saveFile()
    ui.mode = "launcher"
    ui.sel = 1
  elseif key == "s" then saveFile()
  elseif key == "r" then loadFile()
  elseif key == "e" and order[ui.sel] == "mod_file" then ui.edit=true 
  end
end

function love.keyreleased(key)
  if key == "up" or key == "down" or key == "left" or key == "right" then
    if keyRepeat[key] then
      keyRepeat[key].held = false
      keyRepeat[key].timer = 0
    end
  end
end

local function drawSlider(x,y,w,h, value, min, max)
  love.graphics.rectangle("line", x,y,w,h)
  local t = (value - min) / (max - min)
  love.graphics.rectangle("fill", x+1, y+1, (w-2)*t, h-2)
end

function love.draw()
  local W,H = love.graphics.getDimensions()
  local scale = math.min(W/640, H/480)
  love.graphics.push()
  love.graphics.scale(scale, scale)

  if BACKGROUND_IMAGE then
    love.graphics.setColor(1, 1, 1, 1)
    love.graphics.draw(BACKGROUND_IMAGE, 0, 0, 0, 640 / BACKGROUND_IMAGE:getWidth(), 480 / BACKGROUND_IMAGE:getHeight())
  end

  local margin = 20
  local x = margin
  local y = margin

  if ui.mode == "launcher" then
    love.graphics.setColor(0, 0, 0, 0.6)
    love.graphics.rectangle("fill", 0, 0, 640, 480)
    love.graphics.setColor(1, 1, 1, 1)
    
    love.graphics.print("MAX PAYNE LAUNCHER", x, y)
    y = y + 50
    
    for i, option in ipairs(launcherOptions) do
      local isSel = (i == ui.sel)
      local sy = y + (i-1)*40
      
      if isSel then
        love.graphics.setColor(1,1,1,0.3)
        love.graphics.rectangle("fill", x-6, sy-4, 200, 32)
        love.graphics.setColor(1,1,1,1)
      end
      
      love.graphics.print(option, x, sy)
    end
    
    y = y + (#launcherOptions * 40) + 20
    love.graphics.print("A=Select, B=Quit", x, y)
    
  elseif ui.mode == "config" then
    love.graphics.setColor(0, 0, 0, 0.7)
    love.graphics.rectangle("fill", 0, 0, 640, 480)
    love.graphics.setColor(1, 1, 1, 1)
    
    love.graphics.print("CONFIG (A=toggle, ←/→=adjust, START=save, Y=revert, B/BACK=return)", x, y)
    y = y + 30

    for i, key in ipairs(order) do
      local m = meta[key]
      local label = m.label or key
      local isSel = (i == ui.sel)
      local sy = y + (i-1)*28

      if isSel then
        love.graphics.setColor(1,1,1,0.15)
        love.graphics.rectangle("fill", x-6, sy-4, 600, 24)
        love.graphics.setColor(1,1,1,1)
      end

      local val = settings[key]
      local valStr
      if m.type == "int" then
        if m.min==0 and m.max==1 then
          valStr = (val==1) and "ON" or "OFF"
        elseif key == "language" then
          valStr = languageNames[val] or ("Unknown (" .. tostring(val) .. ")")
        else
          valStr = tostring(val)
        end
      elseif m.type == "float" then
        valStr = string.format("%.2f", val)
      else
        valStr = val
      end

      love.graphics.print(string.format("%s", label), x, sy)

      if m.type == "float" then
        drawSlider(280, sy-2, 240, 18, val, m.min, m.max)
        love.graphics.print(valStr, 530, sy)
      else
        love.graphics.print(valStr, 530, sy)
      end

      if m.hint and isSel then
        love.graphics.print(m.hint, x+200, sy+16)
      end
    end

    if ui.edit then
      love.graphics.setColor(0,0,0,0.6)
      love.graphics.rectangle("fill", 10, 300, 620, 160)
      love.graphics.setColor(1,1,1,1)
      love.graphics.rectangle("line", 10, 300, 620, 160)
      love.graphics.print("Edit mod_file (A=add, X=backspace, Y=clear, B=close)", 20, 310)
      love.graphics.print(settings.mod_file or "", 20, 335)

      local bx, by = 20, 365
      for r=1,#kb.rows do
        local row = kb.rows[r]
        for c=1,#row do
          local ch = row:sub(c,c)
          local rx = bx + (c-1)*14
          local ry = by + (r-1)*20
          if r==kb.row and c==kb.col then
            love.graphics.setColor(1,1,1,0.2)
            love.graphics.rectangle("fill", rx-2, ry-2, 14, 18)
            love.graphics.setColor(1,1,1,1)
          end
          love.graphics.print(ch, rx, ry)
        end
      end
    end
  end

  if ui.message_t > 0 then
    local msg = ui.message
    local w = FONT:getWidth(msg) + 20
    love.graphics.setColor(0,0,0,0.5)
    love.graphics.rectangle("fill", 640-w-20, 10, w, 24)
    love.graphics.setColor(1,1,1,1)
    love.graphics.print(msg, 640-w-10, 14)
  end

  love.graphics.pop()
end
