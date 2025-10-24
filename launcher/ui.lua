-- UI renderer for Max Payne launcher
local ui = {}

local FONT
local TITLE_FONT
local BACKGROUND_IMAGE
local LOGO_IMAGE
local CONTROLS_IMAGE

-- Initialize UI resources
function ui.initialize()
    FONT = love.graphics.newFont(14)
    TITLE_FONT = love.graphics.newFont(24)
    love.graphics.setFont(FONT)

    if love.filesystem.getInfo("bg.jpg") then
        BACKGROUND_IMAGE = love.graphics.newImage("bg.jpg")
    end

    if love.filesystem.getInfo("logo.png") then
        LOGO_IMAGE = love.graphics.newImage("logo.png")
    end

    -- Try to load controls image, but don't fail if it's missing
    local controlsFile = io.open("gamedata/data/menu/bitmaps/Menu_bg_Controls.tga", "rb")
    if controlsFile then
        local success, result = pcall(function()
            local controlsFileData = love.filesystem.newFileData(controlsFile:read("*a"), "Menu_bg_Controls.tga.png")
            controlsFile:close()
            return love.graphics.newImage(controlsFileData)
        end)
        if success then
            CONTROLS_IMAGE = result
        else
            if controlsFile then
                controlsFile:close()
            end
        end
    end
end

-- Cleanup function
function ui.cleanup()

end

-- Draw a slider control
local function drawSlider(x, y, w, h, value, min, max)
    love.graphics.rectangle("line", x, y, w, h)
    local t = (value - min) / (max - min)
    love.graphics.rectangle("fill", x + 1, y + 1, (w - 2) * t, h - 2)
end

-- Draw a button with colored circle
local function drawButton(x, y, letter, text, radius)
    radius = radius or 12

    -- Define letter colors
    local colors = {
        A = {1, 0, 0}, -- red
        B = {1, 1, 0}, -- yellow
        Y = {0, 1, 0}, -- green
        X = {0, 0, 1} -- blue
    }

    local letterColor = colors[letter] or {0.5, 0.5, 0.5} -- default gray

    -- Draw dark gray circle fill
    love.graphics.setColor(0.3, 0.3, 0.3, 1) -- dark gray
    love.graphics.circle("fill", x + radius, y + radius, radius)

    -- Draw white circle border
    love.graphics.setColor(1, 1, 1, 1)
    love.graphics.circle("line", x + radius, y + radius, radius)

    -- Draw colored letter
    love.graphics.setColor(letterColor[1], letterColor[2], letterColor[3], 1)
    local font = love.graphics.newFont(radius * 1.2) -- Make font size relative to radius
    local oldFont = love.graphics.getFont()
    love.graphics.setFont(font)

    local letterWidth = font:getWidth(letter)
    local letterHeight = font:getHeight()
    love.graphics.print(letter, x + radius - letterWidth / 2, y + radius - letterHeight / 2)

    -- Reset font and color
    love.graphics.setFont(oldFont)
    love.graphics.setColor(1, 1, 1, 1) -- reset to white

    -- Draw text
    if text then
        love.graphics.print(text, x + radius * 2 + 5, y + radius - 7)
    end

    return radius * 2 + 5 + (text and FONT:getWidth(text) or 0)
end

-- Draw a rounded rectangle button (for START, SELECT, etc.)
local function drawRectButton(x, y, text, label)
    local padding = 8
    local height = 24
    local textWidth = FONT:getWidth(text)
    local width = textWidth + padding * 2
    local radius = 6
    
    -- Draw dark gray rounded rectangle fill
    love.graphics.setColor(0.3, 0.3, 0.3, 1)
    love.graphics.rectangle("fill", x, y, width, height, radius, radius)
    
    -- Draw white rounded rectangle border
    love.graphics.setColor(1, 1, 1, 1)
    love.graphics.rectangle("line", x, y, width, height, radius, radius)
    
    -- Draw button text
    love.graphics.setColor(0.7, 0.7, 0.7, 1)
    love.graphics.print(text, x + padding, y + (height - FONT:getHeight()) / 2)
    
    -- Draw label text
    love.graphics.setColor(1, 1, 1, 1)
    if label then
        love.graphics.print(label, x + width + 10, y + (height - FONT:getHeight()) / 2)
    end
    
    return width + 10 + (label and FONT:getWidth(label) or 0)
end

-- Draw an arrow shape
-- Draw a filled arrowhead (triangle)
local function drawArrow(x, y, direction, size)
    size = size or 12
    local points = {}
    if direction == "left" then
        points = {x + size, y - size / 2, -- top right
        x, y, -- tip (left)
        x + size, y + size / 2 -- bottom right
        }
    elseif direction == "right" then
        points = {x, y - size / 2, -- top left
        x + size, y, -- tip (right)
        x, y + size / 2 -- bottom left
        }
    end
    love.graphics.setColor(1, 1, 1, 1)
    love.graphics.polygon("fill", points)
    love.graphics.setColor(0.3, 0.3, 0.3, 1)
    love.graphics.polygon("line", points)
    return size + 6 -- return width used
end

-- Draw the launcher screen
local function drawLauncher(uiState, launcherOptions)
    local W, H = love.graphics.getDimensions()
    local scale = math.min(W / 640, H / 480)

    local margin = 20
    local x = margin
    local y = margin

    love.graphics.setColor(0, 0, 0, 0.6)
    love.graphics.rectangle("fill", 0, 0, W / scale, H / scale)
    love.graphics.setColor(1, 1, 1, 1)

    -- Draw logo or fallback text
    if LOGO_IMAGE then
        local logoWidth = LOGO_IMAGE:getWidth()
        local logoHeight = LOGO_IMAGE:getHeight()
        local maxWidth = 300 -- Maximum width for the logo
        local maxHeight = 80 -- Maximum height for the logo

        -- Calculate scale to fit within max dimensions
        local logoScale = math.min(maxWidth / logoWidth, maxHeight / logoHeight)
        local scaledWidth = logoWidth * logoScale
        local scaledHeight = logoHeight * logoScale

        love.graphics.draw(LOGO_IMAGE, x - 6, y, 0, logoScale, logoScale)
        y = y + scaledHeight + 30 -- Add some space after logo
    else
        love.graphics.setFont(TITLE_FONT)
        love.graphics.print("MAX PAYNE LAUNCHER", x, y)
        y = y + 50
    end

    love.graphics.setFont(FONT)

    -- Use cached showInstallOption from appState
    local options = uiState.showInstallOption and {"Install Max Payne"} or launcherOptions

    for i, option in ipairs(options) do
        local isSel = (i == uiState.sel)
        local sy = y + (i - 1) * 40

        if isSel then
            love.graphics.setColor(1, 1, 1, 0.3)
            love.graphics.rectangle("fill", x - 6, sy - 7, 200, 32)
            love.graphics.setColor(1, 1, 1, 1)
        end

        love.graphics.print(option, x, sy)
    end

    y = y + (#options * 40) + 20

    -- Render VERSION.txt below menu selections if it exists (using io)
    local versionInfo = nil
    local versionPath = "VERSION.txt"
    local f = io.open(versionPath, "r")
    if f then
        versionInfo = f:read("*a")
        f:close()
    end
    if versionInfo then
        love.graphics.setColor(1, 1, 1, 0.7)
        love.graphics.setFont(FONT)
        local lines = {"Build details:"}
        for line in versionInfo:gmatch("[^\r\n]+") do
            table.insert(lines, line)
        end

        buildInfoY = (H / scale) - (#lines * 18 + 10) - 50
        for i, line in ipairs(lines) do
            love.graphics.print(line, x, buildInfoY + (i - 1) * 18)
        end
        y = y + #lines * 18 + 10
        love.graphics.setColor(1, 1, 1, 1)
    end

    -- Draw button controls with colored circles at the bottom
    local bottomMargin = 50
    local buttonY = (H / scale) - bottomMargin
    local buttonX = x
    buttonX = buttonX + drawButton(buttonX, buttonY, "A", "Select") + 20
    drawButton(buttonX, buttonY, "B", "Quit")
end

-- Draw the controls screen
local function drawControls(uiState)
    love.graphics.origin()
    local W, H = love.graphics.getDimensions()

    --- Draw the screen black
    love.graphics.setColor(0, 0, 0, 1)
    love.graphics.rectangle("fill", 0, 0, W, H)
    love.graphics.setColor(1, 1, 1, 1)

    if CONTROLS_IMAGE then
        local imgW, imgH = CONTROLS_IMAGE:getWidth(), CONTROLS_IMAGE:getHeight()
        local scale = math.min(W / imgW, H / imgH)
        local drawW, drawH = imgW * scale, imgH * scale
        local x = (W - drawW) / 2
        local y = (H - drawH) / 2
        love.graphics.draw(CONTROLS_IMAGE, x, y, 0, scale, scale)
    else
        -- Fallback when controls image is not available
        love.graphics.setColor(1, 1, 1, 1)
        love.graphics.printf("Controls image not available", 0, H / 2 - 10, W, "center")
    end
end

-- Draw the install language selection screen
local function drawInstallLanguageSelect(uiState, config)
    local W, H = 640, 480
    local languageNames = config.getLanguageNames()
    
    -- Title
    love.graphics.setColor(1, 1, 1, 1)
    love.graphics.setFont(TITLE_FONT)
    love.graphics.printf("Select Languages to Install", 0, 40, W, "center")
    love.graphics.setFont(FONT)
    
    -- Language list
    local y = 100
    local languageList = {}
    for i = 0, 6 do
        if languageNames[i] then
            table.insert(languageList, {index = i, name = languageNames[i]})
        end
    end
    
    for i, lang in ipairs(languageList) do
        local isSelected = uiState.sel == i
        local isChecked = uiState.selectedLanguages[lang.index] or false
        
        if isSelected then
            love.graphics.setColor(1, 1, 0, 1) -- Yellow highlight
        else
            love.graphics.setColor(1, 1, 1, 1)
        end
        
        -- Draw checkbox
        local checkboxX = 150
        local checkboxSize = 16
        love.graphics.rectangle("line", checkboxX, y - 2, checkboxSize, checkboxSize)
        
        if isChecked then
            love.graphics.rectangle("fill", checkboxX + 3, y + 1, checkboxSize - 6, checkboxSize - 6)
        end
        
        -- Draw language name
        love.graphics.print(lang.name, checkboxX + checkboxSize + 10, y)
        
        y = y + 25
    end
    
    -- Instructions
    love.graphics.setColor(0.7, 0.7, 0.7, 1)
    local instructY = 380
    drawButton(100, instructY, "A", "Toggle Selection")
    drawButton(100, instructY + 30, "X", "Begin Installation")
    drawButton(100, instructY + 60, "B", "Cancel")
end

-- Draw the installing screen
local function drawInstalling(uiState)
    local W, H = 640, 480
    
    -- Title
    love.graphics.setColor(1, 1, 1, 1)
    love.graphics.setFont(TITLE_FONT)
    if uiState.installInProgress then
        love.graphics.printf("Installing Max Payne...", 0, 40, W, "center")
    elseif uiState.installFailed then
        love.graphics.setColor(1, 0, 0, 1)
        love.graphics.printf("Installation Failed", 0, 40, W, "center")
    else
        love.graphics.setColor(0, 1, 0, 1)
        love.graphics.printf("Installation Complete!", 0, 40, W, "center")
    end
    love.graphics.setFont(FONT)
    
    -- Calculate available space for log lines
    local logStartY = 100
    local logEndY = 400 -- Leave space for buttons at bottom
    local lineHeight = 18
    local availableHeight = logEndY - logStartY
    local maxVisibleLines = math.floor(availableHeight / lineHeight)
    
    -- Log display (as many lines as can fit)
    love.graphics.setColor(1, 1, 1, 1)
    local logLines = {}
    for line in uiState.installLog:gmatch("[^\n]+") do
        table.insert(logLines, line)
    end
    
    -- Show last N lines that fit in the window
    local startLine = math.max(1, #logLines - maxVisibleLines + 1)
    local y = logStartY
    for i = startLine, #logLines do
        local line = logLines[i]
        if #line > 75 then
            line = line:sub(1, 72) .. "..."
        end
        love.graphics.print(line, 50, y)
        y = y + lineHeight
        if y >= logEndY then
            break
        end
    end
    
    -- Instructions
    love.graphics.setColor(0.7, 0.7, 0.7, 1)
    if not uiState.installInProgress then
        if uiState.installFailed then
            drawButton(100, 400, "B", "Exit")
        else
            drawButton(100, 400, "A", "Continue")
        end
    end
end

-- Draw the no files error screen
local function drawNoFiles(uiState)
    local W, H = 640, 480
    
    love.graphics.setColor(1, 1, 1, 1)
    love.graphics.setFont(TITLE_FONT)
    love.graphics.printf("Max Payne Gamedata Not Found", 0, 100, W, "center")
    
    love.graphics.setFont(FONT)
    love.graphics.setColor(1, 0.4, 0.4, 1) -- Light red for error
    
    -- Display the error message from isGameInstalledCorrectly
    local errorMsg = uiState.installError or "Game files are missing or incomplete"
    love.graphics.printf(errorMsg, 50, 180, W - 100, "center")
    
    love.graphics.setColor(0.9, 0.9, 0.9, 1)
    
    -- Get current working directory
    local cwd = love.filesystem.getWorkingDirectory()
    local message = string.format("\n\nPlace Max Payne Mobile (1.7 or newer) APK and OBB files inside:\n\n%s", cwd)
    
    love.graphics.printf(message, 50, 220, W - 100, "center")
    
    -- Instructions
    love.graphics.setColor(0.7, 0.7, 0.7, 1)
    drawButton(180, 380, "A", "Recheck Files")
    drawButton(180, 410, "B", "Exit")
end

-- Draw the config screen
local function drawConfig(uiState, config)
    local W, H = love.graphics.getDimensions()
    local scale = math.min(W / 640, H / 480)
    local settings = config.getSettings()
    local meta = config.getMeta()
    local order = config.getOrder()

    local margin = 20
    local x = margin
    local y = margin

    love.graphics.setColor(0, 0, 0, 0.7)
    love.graphics.rectangle("fill", 0, 0, W / scale, H / scale)
    love.graphics.setColor(1, 1, 1, 1)

    -- Draw config controls with colored button circles
    local controlY = y
    love.graphics.print("SETTINGS", x, controlY)
    y = y + 30
    -- Draw button hints at the bottom
    local bottomMargin = 50
    local buttonY = (H / scale) - bottomMargin
    local buttonX = x
    buttonX = buttonX + drawButton(buttonX, buttonY - 5, "A", "Toggle") + 15
    -- Vertically center arrows to match text baseline
    -- Draw arrows aligned with text baseline
    local textBaseline = buttonY
    local arrowOffset = FONT:getHeight() * 0.35 + 1
    buttonX = buttonX + drawArrow(buttonX, textBaseline + arrowOffset, "left") + 2
    buttonX = buttonX + drawArrow(buttonX, textBaseline + arrowOffset, "right") + 2
    love.graphics.setColor(1, 1, 1, 1)
    love.graphics.print("Adjust", buttonX, textBaseline)
    buttonX = buttonX + FONT:getWidth("Adjust") + 15
    buttonX = buttonX + drawButton(buttonX, buttonY - 5, "Y", "Revert") + 15
    buttonX = buttonX + drawButton(buttonX, buttonY - 5, "X", "Defaults") + 15
    buttonX = buttonX + drawButton(buttonX, buttonY - 5, "B", "Return")

    for i, key in ipairs(order) do
        local m = meta[key]
        local label = m.label or key
        local isSel = (i == uiState.sel)
        local sy = y + (i - 1) * 28

        if isSel then
            love.graphics.setColor(1, 1, 1, 0.15)
            love.graphics.rectangle("fill", x - 6, sy - 4, 600, 24)
            love.graphics.setColor(1, 1, 1, 1)
        end

        local valStr = config.formatValue(key)

        love.graphics.print(string.format("%s", label), x, sy)

        if m.type == "float" then
            drawSlider(280, sy - 2, 240, 18, settings[key], m.min, m.max)
            love.graphics.print(valStr, 530, sy)
        else
            love.graphics.print(valStr, 530, sy)
        end

        if m.hint and isSel then
            love.graphics.print(m.hint, x + 200, sy + 16)
        end
    end
end

-- Draw message overlay
local function drawMessage(uiState)
    if uiState.message_t > 0 then
        local msg = uiState.message
        local w = FONT:getWidth(msg) + 20
        love.graphics.setColor(0, 0, 0, 0.5)
        love.graphics.rectangle("fill", 640 - w - 20, 10, w, 24)
        love.graphics.setColor(1, 1, 1, 1)
        love.graphics.print(msg, 640 - w - 10, 14)
    end
end

-- Main draw function
function ui.draw(uiState, config, launcherOptions)
    local W, H = love.graphics.getDimensions()

    if BACKGROUND_IMAGE then
        love.graphics.setColor(1, 1, 1, 1)
        local bgWidth = BACKGROUND_IMAGE:getWidth()
        local bgHeight = BACKGROUND_IMAGE:getHeight()

        local scaleX = W / bgWidth
        local scaleY = H / bgHeight
        local scale = math.max(scaleX, scaleY) -- Use max to ensure full coverage

        local scaledWidth = bgWidth * scale
        local scaledHeight = bgHeight * scale
        local offsetX = (W - scaledWidth) / 2
        local offsetY = (H - scaledHeight) / 2

        love.graphics.draw(BACKGROUND_IMAGE, offsetX, offsetY, 0, scale, scale)
    end

    local scale = math.min(W / 640, H / 480)
    love.graphics.push()
    love.graphics.scale(scale, scale)

    -- Draw current screen
    if uiState.mode == "launcher" then
        drawLauncher(uiState, launcherOptions)
    elseif uiState.mode == "config" then
        drawConfig(uiState, config)
    elseif uiState.mode == "controls" then
        drawControls(uiState)
    elseif uiState.mode == "install_language_select" then
        drawInstallLanguageSelect(uiState, config)
    elseif uiState.mode == "installing" then
        drawInstalling(uiState)
    elseif uiState.mode == "no_files" then
        drawNoFiles(uiState)
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
