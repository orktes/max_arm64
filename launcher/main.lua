-- Max Payne Launcher - Main Entry Point
local config = require("config")
local audio = require("audio")
local input = require("input")
local ui = require("ui")

-- Application state
local appState = {
    sel = 1,
    edit = false,
    message = "",
    message_t = 0,
    mode = "launcher",
    selectedLanguages = {},
    installLog = "",
    installInProgress = false,
    installFailed = false,
    showInstallOption = false,
    gameInstalled = false,
    installError = ""
}

local launcherOptions = {"Start Game", "Settings", "Controls"}
local installOptions = {"Install Max Payne"}

-- Utility functions
local function clamp(v, a, b)
    return math.max(a, math.min(b, v))
end

local function setMessage(msg, time)
    appState.message = msg
    appState.message_t = time
end

local function fileExists(path)
    local f = io.open(path, "r")
    if f then
        f:close()
        return true
    end
    return false
end

local function fileIsDirectory(path)
    local f = io.open(path, "r")
    if f then
        local ok, err, code = f:read(1)
        f:close()
        if code == 21 then -- EISDIR
            return true
        end
    end
    return false
end

local function isAPKOrOBBPresent()
    --- Check if there is a file that ends with .apk or .obb in the current directory
    local p = io.popen('ls *.apk *.obb 2> /dev/null')
    local result = p:read('*a')
    p:close()
    if result ~= "" then
        return true
    end
    return false
end

local function isGameInstalledCorrectly()
    -- Check for libMaxPayne.so in current directory
    if not fileExists("libMaxPayne.so") then
        return false, "libMaxPayne.so not found in current directory"
    end
    
    -- Required game data files (matching src/main.c check_data function)
    local requiredFiles = {
        "gamedata/MaxPayneSoundsv2.msf",
        "gamedata/x_data.ras",
        "gamedata/x_english.ras", 
        "gamedata/x_level1.ras",
        "gamedata/x_level2.ras",
        "gamedata/x_level3.ras",
        "gamedata/data",
        "gamedata/es2",
        "gamedata/es2/DefaultPixel.txt"  -- This indicates assets folder has been merged
    }
    
    -- Check each required file/directory
    for _, file in ipairs(requiredFiles) do
        if not fileExists(file) and not fileIsDirectory(file) then
            return false, "Missing required file or directory: " .. file
        end
    end
    
    return true, "Game installation verified successfully"
end

-- Game control functions
local function launchGame()
    love.event.quit(0)
end

local function moveSel(delta)
    if appState.mode == "launcher" then
        local options = appState.showInstallOption and installOptions or launcherOptions
        appState.sel = clamp(appState.sel + delta, 1, #options)
    elseif appState.mode == "config" then
        appState.sel = clamp(appState.sel + delta, 1, #config.getOrder())
    elseif appState.mode == "install_language_select" then
        local languageNames = config.getLanguageNames()
        local languageCount = 0
        for _ in pairs(languageNames) do
            languageCount = languageCount + 1
        end
        appState.sel = clamp(appState.sel + delta, 1, languageCount)
    end
    audio.playMenuSound("move")
end

local function adjustValue(key, dir)
    local result = config.adjustValue(key, dir)
    if result == true then
        audio.playMenuSound("slider")
    elseif result == "edit" then
        appState.edit = true
    end
end

local function toggleValue(key)
    if config.toggleValue(key) then
        audio.playMenuSound("select")
    end
end

local function saveConfig()
    local msg, time = config.save()
    setMessage(msg, time)
end

local function loadConfig()
    local msg, time = config.load()
    setMessage(msg, time)
end

local function enterSettings()
    audio.playMenuSound("select")
    appState.mode = "config"
    appState.sel = 1
end

local function enterControls()
    audio.playMenuSound("select")
    appState.mode = "controls"
    appState.sel = 2
end

local function exitApp()
    love.event.quit(1)
end

local function recheckGameFiles()
    audio.playMenuSound("select")
    
    -- Recheck installation state
    appState.showInstallOption = isAPKOrOBBPresent()
    local installed, errorMsg = isGameInstalledCorrectly()
    appState.gameInstalled = installed
    appState.installError = errorMsg or ""
    
    -- Update mode based on what we found
    if appState.gameInstalled then
        -- Game is now properly installed, go to launcher
        appState.mode = "launcher"
        appState.sel = 1
        setMessage("Game files detected!", 2.0)
    elseif appState.showInstallOption then
        -- APK/OBB found, show install option
        appState.mode = "launcher"
        appState.sel = 1
        setMessage("Installation files found", 2.0)
    else
        -- Still nothing found
        setMessage("No game files found", 2.0)
    end
end

local function saveAndExit()
    saveConfig()
    appState.mode = "launcher"
    appState.sel = 1
end

local function enterEdit()
    appState.edit = true
end

local function exitEdit()
    appState.edit = false
end

local function returnDefaultConfigs()
    config.returnDefaults()
    setMessage("Restored default settings", 2.0)
end

local function enterInstallLanguageSelect()
    audio.playMenuSound("select")
    appState.mode = "install_language_select"
    appState.sel = 1
    -- Initialize selected languages with English enabled by default
    appState.selectedLanguages = {[0] = true} -- English is index 0
end

local function toggleLanguageSelection()
    local languageNames = config.getLanguageNames()
    local languageIndex = appState.sel - 1 -- Convert 1-based to 0-based
    appState.selectedLanguages[languageIndex] = not appState.selectedLanguages[languageIndex]
    audio.playMenuSound("select")
end

local function startInstallation()
    audio.playMenuSound("select")
    appState.mode = "installing"
    appState.installLog = ""
    appState.installInProgress = true
    appState.installFailed = false
    
    -- Build language arguments using language names
    local languageArgs = {}
    local languageNames = config.getLanguageNames()
    for langIndex, selected in pairs(appState.selectedLanguages) do
        if selected and languageNames[langIndex] then
            table.insert(languageArgs, languageNames[langIndex])
        end
    end
    
    -- Check for patchscript location (dev vs production)
    local patchscriptPath = nil
    if fileExists("port/tools/patchscript") then
        patchscriptPath = "port/tools/patchscript"
    elseif fileExists("tools/patchscript") then
        patchscriptPath = "tools/patchscript"
    end
    
    if not patchscriptPath then
        appState.installLog = "Error: patchscript not found in port/tools/ or tools/"
        appState.installInProgress = false
        appState.installFailed = true
        return
    end
    
    -- Start the installation process
    local handle = io.popen("bash " .. patchscriptPath .. " " .. table.concat(languageArgs, " ") .. " 2>&1", "r")
    if handle then
        appState.installHandle = handle
    else
        appState.installLog = "Failed to start installation script"
        appState.installInProgress = false
        appState.installFailed = true
    end
end

local function cancelInstallation()
    appState.mode = "launcher"
    appState.sel = 1
    appState.selectedLanguages = {}
end

local function finishInstallation()
    if not appState.installFailed then
        -- Reinitialize audio and UI to load new resources
        audio.cleanup()
        ui.cleanup()
        ui.initialize()
        audio.initialize()
        
        -- Refresh language availability cache
        config.refreshLanguageCache()
        
        -- Recheck installation state
        appState.showInstallOption = isAPKOrOBBPresent()
        local installed, errorMsg = isGameInstalledCorrectly()
        appState.gameInstalled = installed
        appState.installError = errorMsg or ""
        
        -- If neither APK/OBB nor game installed, show error mode
        if not appState.showInstallOption and not appState.gameInstalled then
            appState.mode = "no_files"
        else
            appState.mode = "launcher"
        end
    else
        appState.mode = "launcher"
    end
    
    appState.sel = 1
    appState.installLog = ""
    appState.installInProgress = false
    appState.installFailed = false
end

-- Love2D callbacks
function love.load()
    love.window.setTitle("Max Payne")
    love.window.setMode(640, 480, {
        resizable = true,
        vsync = 1
    })

    -- Check once if APK/OBB files are present
    appState.showInstallOption = isAPKOrOBBPresent()
    
    -- Check if game is installed correctly
    local installed, errorMsg = isGameInstalledCorrectly()
    appState.gameInstalled = installed
    appState.installError = errorMsg or ""
    
    -- If neither APK/OBB nor game installed, show error mode
    if not appState.showInstallOption and not appState.gameInstalled then
        appState.mode = "no_files"
    end

    -- Initialize modules
    ui.initialize()
    audio.initialize()

    -- Setup input callbacks
    input.setCallbacks({
        moveSel = moveSel,
        adjustValue = adjustValue,
        launchGame = launchGame,
        enterSettings = enterSettings,
        enterControls = enterControls,
        exitApp = exitApp,
        saveAndExit = saveAndExit,
        toggleValue = toggleValue,
        saveConfig = saveConfig,
        loadConfig = loadConfig,
        enterEdit = enterEdit,
        returnDefaultConfigs = returnDefaultConfigs,
        exitEdit = exitEdit,
        enterInstallLanguageSelect = enterInstallLanguageSelect,
        toggleLanguageSelection = toggleLanguageSelection,
        startInstallation = startInstallation,
        cancelInstallation = cancelInstallation,
        finishInstallation = finishInstallation,
        recheckGameFiles = recheckGameFiles
    })

    -- Load initial config
    local msg, time = config.load()
    setMessage(msg, time)
end

function love.quit()
    audio.cleanup()
end

function love.update(dt)
    -- Store order in appState for input module
    appState.order = config.getOrder()

    -- Handle installation log updates
    if appState.installInProgress and appState.installHandle then
        local line = appState.installHandle:read("*l")
        if line then
            appState.installLog = appState.installLog .. line .. "\n"
        else
            -- Process finished
            local success = appState.installHandle:close()
            appState.installHandle = nil
            appState.installInProgress = false
            if not success then
                appState.installFailed = true
            end
        end
    end

    input.update(dt, appState, config.getOrder())
    audio.update(dt)
    ui.updateMessage(appState, dt)
end

function love.gamepadpressed(joy, button)
    input.gamepadPressed(joy, button, appState, launcherOptions)
end

function love.gamepadreleased(joy, button)
    input.gamepadReleased(joy, button)
end

function love.keypressed(key)
    input.keyPressed(key, appState, launcherOptions)
end

function love.keyreleased(key)
    input.keyReleased(key)
end

function love.draw()
    ui.draw(appState, config, launcherOptions)
end
