-- UI renderer for Max Payne launcher
local ui = {}

local FONT
local TITLE_FONT
local BACKGROUND_IMAGE

-- Initialize UI resources
function ui.initialize()
  FONT = love.graphics.newFont(14)
  TITLE_FONT = love.graphics.newFont(24)
  love.graphics.setFont(FONT)
  
  if love.filesystem.getInfo("bg.jpg") then
    BACKGROUND_IMAGE = love.graphics.newImage("bg.jpg")
  end
end

-- Draw a slider control
local function drawSlider(x, y, w, h, value, min, max)
  love.graphics.rectangle("line", x, y, w, h)
  local t = (value - min) / (max - min)
  love.graphics.rectangle("fill", x+1, y+1, (w-2)*t, h-2)
end

-- Draw the launcher screen
local function drawLauncher(uiState, launcherOptions)
  local W, H = love.graphics.getDimensions()
  local scale = math.min(W/640, H/480)
  
  local margin = 20
  local x = margin
  local y = margin

  love.graphics.setColor(0, 0, 0, 0.6)
  love.graphics.rectangle("fill", 0, 0, 640, 480)
  love.graphics.setColor(1, 1, 1, 1)
  love.graphics.setFont(TITLE_FONT)
  love.graphics.print("MAX PAYNE LAUNCHER", x, y)
  love.graphics.setFont(FONT)
  y = y + 50
  
  for i, option in ipairs(launcherOptions) do
    local isSel = (i == uiState.sel)
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
end

-- Draw the config screen
local function drawConfig(uiState, config)
  local settings = config.getSettings()
  local meta = config.getMeta()
  local order = config.getOrder()
  
  local margin = 20
  local x = margin
  local y = margin

  love.graphics.setColor(0, 0, 0, 0.7)
  love.graphics.rectangle("fill", 0, 0, 640, 480)
  love.graphics.setColor(1, 1, 1, 1)
  
  love.graphics.print("CONFIG (A=toggle, <-/->=adjust, START=save, Y=revert, B/BACK=return)", x, y)
  y = y + 30

  for i, key in ipairs(order) do
    local m = meta[key]
    local label = m.label or key
    local isSel = (i == uiState.sel)
    local sy = y + (i-1)*28

    if isSel then
      love.graphics.setColor(1,1,1,0.15)
      love.graphics.rectangle("fill", x-6, sy-4, 600, 24)
      love.graphics.setColor(1,1,1,1)
    end

    local valStr = config.formatValue(key)

    love.graphics.print(string.format("%s", label), x, sy)

    if m.type == "float" then
      drawSlider(280, sy-2, 240, 18, settings[key], m.min, m.max)
      love.graphics.print(valStr, 530, sy)
    else
      love.graphics.print(valStr, 530, sy)
    end

    if m.hint and isSel then
      love.graphics.print(m.hint, x+200, sy+16)
    end
  end
end

-- Draw message overlay
local function drawMessage(uiState)
  if uiState.message_t > 0 then
    local msg = uiState.message
    local w = FONT:getWidth(msg) + 20
    love.graphics.setColor(0,0,0,0.5)
    love.graphics.rectangle("fill", 640-w-20, 10, w, 24)
    love.graphics.setColor(1,1,1,1)
    love.graphics.print(msg, 640-w-10, 14)
  end
end

-- Main draw function
function ui.draw(uiState, config, launcherOptions)
  local W, H = love.graphics.getDimensions()
  local scale = math.min(W/640, H/480)
  love.graphics.push()
  love.graphics.scale(scale, scale)

  -- Draw background
  if BACKGROUND_IMAGE then
    love.graphics.setColor(1, 1, 1, 1)
    love.graphics.draw(BACKGROUND_IMAGE, 0, 0, 0, 640 / BACKGROUND_IMAGE:getWidth(), 480 / BACKGROUND_IMAGE:getHeight())
  end

  -- Draw current screen
  if uiState.mode == "launcher" then
    drawLauncher(uiState, launcherOptions)
  elseif uiState.mode == "config" then
    drawConfig(uiState, config)
  end

  -- Draw message overlay
  drawMessage(uiState)

  love.graphics.pop()
end

-- Update message timer
function ui.updateMessage(uiState, dt)
  if uiState.message_t > 0 then 
    uiState.message_t = math.max(0, uiState.message_t - dt) 
  end
end

return ui