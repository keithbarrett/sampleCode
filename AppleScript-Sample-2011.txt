--
-- © Keith Barrett (KeithBarrett.com) 2011
-- Version 2.5 3/19/2011
--
--
-- Based on original concept by Mark Johnson www.ttfn.tv 2010
--
-- You are free to use and modify this for your own non-commercial use.
--

on runme(message)
	if (item 1 of message = 176) then -- All Nano messages start with 176
		
		global initGlobals
		global B9t -- Is the 9th top button on/off?
		global B9b -- Is the 9th bottom buton of/off?
		global knob9Bg -- Last background value seen on the 9th nobe
		global knob9Shot -- Last normal value seen on the 9th knob
		global knob9Ov -- Last overlay value seen on the 9th knob
		global groupL3rdNbr -- L3rd the group slider is mapped to
		global groupSliderOn -- Is the group slider is up?
		global groupShotNbr -- SHOT the group button is mapped to
		global groupShotBgNbr -- BG the group button is mapped to
		global groupShotFgNbr -- FG the group button is mapped to
		global lastSliderUp -- What group # did we last moved a slider up?
		global knobL3rd -- The last Fg knob value seen (0 - 21)
		global sceneNbr -- Korg scene # (1-3)
		
		initMemory()
		set item2 to item 2 of message
		set item3 to item 3 of message
		
		-- Determine which scene we are on. set sceneNbr to 4 (not used)
		set sceneNbr to 3
		if (item2 < 85) then set sceneNbr to 2
		if (item2 < 43) then set sceneNbr to 1
		set item2 to item2 - ((sceneNbr - 1) * 42)
		
		-- Determine which groupNbr/column we are on
		set groupNbr to 0
		if (item2 > 0) and (item2 < 10) then set groupNbr to (item2 - 0) ---- 1-9 are the sliders
		if (item2 > 9) and (item2 < 19) then set groupNbr to (item2 - 9) ---- 10-18 are the knobs
		if (item2 > 18) and (item2 < 28) then set groupNbr to (item2 - 18) -- 19-27 are the top buttons
		if (item2 > 27) and (item2 < 37) then set groupNbr to (item2 - 27) -- 28-36 are the lower buttons
		set groupNbrIndex to groupNbr
		
		
		-- ### TRANSPORT BUTTONS ### --
		
		if (groupNbr = 0) then
			if (item2 = 37) and (item3 = 127) then intro() -- Rewind (Intro)
			if (item2 = 38) and (item3 = 127) then wirecastGo() -- Play (Go)
			if (item2 = 39) and (item3 = 127) then outro() -- FF (Outro)
			if (item2 = 40) then setBroadcast(item3 = 127) -- Loop/circle (Broadcast) ON/OFF
			if (item2 = 41) then setAutoLive(item3 = 127) -- Stop (Auto Live) ON/OFF
			if (item2 = 42) then setRecord(item3 = 127) -- Record ON/OFF
		end if
		
		
		-- ### Group 1 - 8 CONTROLS ### --
		
		if (groupNbr > 0) and (groupNbr < 9) then
			if (item2 = (groupNbr + 18)) and (item3 = 127) then doButtonPress(groupNbr, groupNbr) -- groupNbr Top Button pressed
			if (item2 = (groupNbr + 27)) and (item3 = 127) then doButtonPress(groupNbr, groupNbr + 8) -- groupNbr Bottom Button pressed
			if (item2 < 9) and (item3 = 0) then doSlider(groupNbr, false) -- Slider down
			if (item2 < 9) and (item3 > 1) then doSlider(groupNbr, true) -- Slider up
			-- Note we do the slider as 2 if statements to avoid processing any in between item3 values
			
			if (item2 = (groupNbr + 9)) then -- Shot Knob turned
				set newValue to item3 -- Gives us a value between 0 and 21
				set oldValue to knobL3rd
				
				if (B9t or B9b) then -- programming mode
					if (oldValue is not equal to newValue) then
						setShot("L3rd", newValue, "foreground", true)
					end if
				else -- L3rd mode
					if (newValue = 0) then set newValue to groupNbr
					if (oldValue is not equal to newValue) and (item groupNbr in groupSliderOn) then
						set item groupNbr in groupL3rdNbr to newValue
						setShot("L3rd", newValue, "foreground", true)
						set lastSliderUp to groupNbr
					end if
				end if
			end if
		end if
		
		
		-- ### Group 9 CONTROLS ### --
		
		if (groupNbr = 9) then
			if (item2 = 9) then -- Transition speed and type slider moved
				if (item3 = 0) then bowString()
				if (item3 > 0) and (item3 < 26) then setSpeed("slowest")
				if (item3 > 25) and (item3 < 52) then setSpeed("slow")
				if (item3 > 51) and (item3 < 78) then setSpeed("normal")
				if (item3 > 77) and (item3 < 104) then setSpeed("fast")
				if (item3 > 103) and (item3 < 127) then setSpeed("fastest")
				if (item3 = 127) then selectCut()
			end if
			
			if (item2 = 27) then set B9t to (item3 = 127) -- 9th Top button pressed
			if (item2 = 36) then set B9b to (item3 = 127) -- 9th Bottom button pressed
			
			-- Knob 9
			if (item2 = 18) then -- Knob 9 turned
				set knob9 to item3 -- Gives us a value between 0 and 21
				processKnob9(knob9)
			end if
		end if
	end if
	
end runme



-- -- -- -- -- -- Supporting Methods -- -- -- -- -- --

on initMemory()
	global initGlobals
	global debugOn -- vocalize internal errors
	global autoLive -- Is auto live on/off?
	global B9t -- Is the 9th top button on/off?
	global B9b -- Is the 9th bottom buton of/off?
	global knob9Bg -- Last background value seen on the 9th nobe
	global knob9Shot -- Last normal value seen on the 9th knob
	global knob9Ov -- Last overlay value seen on the 9th knob
	global groupL3rdNbr -- L3rd the group slider is mapped to
	global groupSliderOn -- Is the group slider is up?
	global groupShotNbr -- SHOT the group button is mapped to
	global groupShotBgNbr -- BG the group button is mapped to
	global groupShotFgNbr -- FG the group button is mapped to
	global lastSliderUp -- What group # did we last moved a slider up?
	global lastShotButton -- Last group button pressed
	global knobL3rd -- The last Fg knob value seen (0 - 21)
	global sceneNbr -- Korg scene # (1-3)
	global recordOn -- Is the record button on?
	
	try
		set initGlobals to initGlobals + 1
	on error
		say "Korg Control initialized"
		set groupShotFgNbr to {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
		set groupL3rdNbr to {1, 2, 3, 4, 5, 6, 7, 8}
		set groupSliderOn to {false, false, false, false, false, false, false, false}
		set lastSliderUp to 0
		set groupShotNbr to {1, 2, 3, 4, 5, 6, 7, 8, 22, 23, 24, 25, 26, 27, 28, 29}
		set groupShotBgNbr to {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
		set B9t to false
		set B9b to false
		set autoLive to false
		set debugOn to false
		set knob9Shot to 0
		set knob9Bg to 0
		set knob9Ov to 0
		set lastShotButton to {0, 0} -- preview, program (0 - 16)
		set knobL3rd to 0
		set sceneNbr to 1
		set recordOn to false
		
		displayBlankShot()
		doButtonPress(1, 1)
	end try
	set initGlobals to 1
end initMemory



on setAutoLive(newState)
	global autoLive
	global B9t
	global B9b
	
	if (newState) then
		set wirecastAutoLive to 1
	else
		set wirecastAutoLive to 0
	end if
	
	try
		if (not B9t) and (not B9b) then
			tell application "Wirecast-5.0.3"
				set myDoc to last document
				set the auto live of myDoc to wirecastAutoLive
			end tell
		end if
		set autoLive to newState
	on error
		say "Error"
	end try
end setAutoLive



on intro()
	global sceneNbr
	
	set shotName to "Intro"
	if (sceneNbr > 1) then set shotName to shotName & "-" & sceneNbr
	
	tell application "Wirecast-5.0.3"
		set myDoc to last document
		set desired_shot to the shot named shotName of myDoc
		set normal_layer to the layer named "normal" of myDoc
		
		set the audio muted to speaker of myDoc to 0
		set the active shot of normal_layer to the desired_shot
		
		-- set overlay_layer to the layer named "overlay" of myDoc
		-- set mute_audio to the shot named "Blank Shot" of myDoc
		-- set the active shot of overlay_layer to the mute_audio
	end tell
end intro



on outro()
	global sceneNbr
	
	set shotName to "Outro"
	if (sceneNbr > 1) then set shotName to shotName & "-" & sceneNbr
	
	try
		tell application "Wirecast-5.0.3"
			set myDoc to last document
			set desired_shot to the shot named shotName of myDoc
			set normal_layer to the layer named "normal" of myDoc
			
			set the audio muted to speaker of myDoc to 0
			set the active shot of normal_layer to the desired_shot
			
			-- set overlay_layer to the layer named "overlay" of myDoc
			-- set mute_audio to the shot named "Blank Shot" of myDoc
			-- set the active shot of overlay_layer to the mute_audio
		end tell
	end try
end outro



on wirecastGo()
	global autoLive
	global lastShotButton
	global B9t
	global B9b
	global debugOn
	
	try
		set oldProgram to item 2 in lastShotButton
		set groupNbr to oldProgram
		if (groupNbr is greater than 8) then set groupNbr to oldProgram - 8
		
		if (not B9t) and (not B9b) then -- precaution
			tell application "Wirecast-5.0.3"
				set myDoc to last document
				go myDoc
			end tell
			
			set item 2 in lastShotButton to (item 1 in lastShotButton)
			
			if (not autoLive) then
				displayBlankShot()
				set item 1 in lastShotButton to 0
			else -- AutoLive AND Go pressed means swap displays
				if (oldProgram is not equal to (item 2 in lastShotButton)) then
					setAutoLive(false)
					displayWholeShot(groupNbr, oldProgram)
					set item 1 in lastShotButton to oldProgram
					setAutoLive(true)
				end if
			end if
		end if
	on error
		if (debugOn) then say "Go error"
	end try
end wirecastGo



on setRecord(turnOn)
	global B9t
	global B9b
	global recordOn
	
	try
		if (turnOn) then
			if (not B9t) and (not B9b) then
				tell application "Wirecast-5.0.3"
					set myDoc to last document
					start recording myDoc
				end tell
			end if
			set recordOn to true
		else
			if (not B9t) and (not B9b) then
				tell application "Wirecast-5.0.3"
					set myDoc to last document
					stop recording myDoc
				end tell
			end if
			set recordOn to false
		end if
	on error
		if (debugOn) then say "Record Error"
	end try
end setRecord



on setBroadcast(turnOn)
	try
		if (turnOn) then
			tell application "Wirecast-5.0.3"
				set myDoc to last document
				start broadcasting myDoc
			end tell
		else
			tell application "Wirecast-5.0.3"
				set myDoc to last document
				stop broadcasting myDoc
			end tell
		end if
	on error
		say "Broadcast Error"
	end try
end setBroadcast



on setShot(prefix, shotNbr, layerName, blankIt)
	global sceneNbr
	global knobL3rd
	global knob9Shot
	global knob9Bg
	
	if (shotNbr > 0) then
		set suffix to ""
		
		if (prefix = "Shot") then
			set knob9Shot to shotNbr
			if (shotNbr > 21) then
				set suffix to "b"
				set shotNbr to shotNbr - 21
			else
				set suffix to "a"
			end if
		end if
		
		if (sceneNbr > 1) then set suffix to suffix & "-" & sceneNbr
		
		set shotName to prefix & " " & shotNbr & suffix
		if (prefix = "L3rd") then set knobL3rd to shotNbr
		if (prefix = "Bg") then set knob9Bg to shotNbr
	else
		set shotName to "Blank Shot"
		if (prefix = "L3rd") then set knobL3rd to 0
		if (prefix = "Bg") then set knob9Bg to 0
	end if
	
	try
		tell application "Wirecast-5.0.3"
			set myDoc to last document
			set desired_shot to the shot named shotName of myDoc
			set normal_layer to the layer named layerName of myDoc
			set the active shot of normal_layer to the desired_shot
		end tell
	on error
		if (blankIt) then -- Display Blank if shot fails (optional)
			tell application "Wirecast-5.0.3"
				set myDoc to last document
				set desired_shot to the shot named "Blank Shot" of myDoc
				set normal_layer to the layer named layerName of myDoc
				set the active shot of normal_layer to the desired_shot
			end tell
		end if
	end try
end setShot



on displayWholeShot(groupNbr, groupNbrIndex)
	global debugOn
	global groupSliderOn
	global groupShotBgNbr
	global groupShotFgNbr
	global groupShotNbr
	global groupL3rdNbr
	global groupSliderOn
	global lastSliderUp
	global sceneNbr
	global lastShotButton
	
	try
		if (groupNbrIndex > 0) then
			setShot("Bg", item groupNbrIndex in groupShotBgNbr, "background", false)
			
			set shotNbr to 0
			if (lastSliderUp > 0) then set shotNbr to item lastSliderUp in groupL3rdNbr
			if (item groupNbr in groupSliderOn) then set shotNbr to item groupNbr in groupL3rdNbr
			if (item groupNbrIndex in groupShotFgNbr > 0) then set shotNbr to item groupNbrIndex in groupShotFgNbr
			setShot("L3rd", shotNbr, "foreground", true)
			
			set shotNbr to item groupNbrIndex in groupShotNbr
			setShot("Shot", shotNbr, "normal", false)
			set item 1 in lastShotButton to shotNbr
		else
			displayBlankShot()
			set item 1 in lastShotButton to 0
		end if
	on error
		if (debugOn) then say "Unexpected error in display whole shot"
	end try
end displayWholeShot



on displayBlankShot()
	global knob9Shot
	global knob9Bg
	global knobL3rd
	
	setShot("L3rd", 0, "foreground", false)
	setShot("Bg", 0, "background", false)
	setShot("Shot", 0, "normal", false)
	set knob9Shot to 0
	set knob9Bg to 0
	set knobL3rd to 0
end displayBlankShot



on doButtonPress(groupNbr, groupNbrIndex)
	global debugOn
	global B9t
	global B9b
	global groupShotBgNbr
	global groupShotNbr
	global groupShotFgNbr
	global knob9Shot
	global knob9Bg
	global lastShotButton
	global knobL3rd
	global recordOn
	global autoLive
	
	set doSave to false
	set doLoad to false
	set doMapping to true
	
	if (B9t and B9b and recordOn) then set doSave to true
	if (B9t and B9b and autoLive) then set doLoad to true
	if (item 1 in lastShotButton = 0) then set doMapping to false
	
	if (doSave or doLoad) then
		if (doSave) then saveConfig(groupNbrIndex)
		if (doLoad) then loadConfig(groupNbrIndex)
	else
		try
			if (B9t or B9b) and (doMapping) then -- We're mapping
				-- if (knob9Shot > 0) then 
				set item groupNbrIndex of groupShotNbr to knob9Shot
				set item groupNbrIndex of groupShotBgNbr to knob9Bg
				set item groupNbrIndex of groupShotFgNbr to knobL3rd
				set knob9Shot to 0
				set knob9Bg to 0
				displayWholeShot(0, 0) -- Indicate map is done
				set item 1 in lastShotButton to 0
			else
				displayWholeShot(groupNbr, groupNbrIndex)
				set item 1 in lastShotButton to groupNbrIndex
				-- ### Need method to set to "factory" default ot blank shot ###
			end if
		on error
			if (debugOn) then say "Unexpected error in do button"
		end try
	end if
end doButtonPress



on doSlider(groupNbr, state)
	global groupSliderOn
	global groupL3rdNbr
	global lastSliderUp
	global lastShotButton
	global groupShotFgNbr
	
	set lastButton to item 1 in lastShotButton
	
	set lastButtongroupNbr to lastButton
	if (lastButtongroupNbr > 8) then set lastButtongroupNbr to lastButtongroupNbr - 8
	
	if (lastButton > 0) then
		set fgNbr to item lastButton in groupShotFgNbr
		set L3rdNbr to item lastButtongroupNbr in groupL3rdNbr
	else
		set fgNbr to 0
		set L3rdNbr to 0
	end if
	
	try
		if (state) then -- Turn ON L3rd
			
			set lastSliderUp to groupNbr
			set L3rdNbr to item groupNbr in groupL3rdNbr
			
			if (fgNbr = 0) then
				setShot("L3rd", L3rdNbr, "foreground", true)
			else
				setShot("L3rd", fgNbr, "foreground", true)
			end if
			
		else -- Turn L3rd OFF
			
			if (groupNbr = lastButtongroupNbr) then -- It's our slider
				set L3rdNbr to 0
				if (groupNbr = lastSliderUp) then -- and we were also last
					set lastSliderUp to 0
					setShot("L3rd", fgNbr, "foreground", true)
				else -- we're not the last up
					if (fgNbr > 0) then
						setShot("L3rd", fgNbr, "foreground", true)
					else
						if (lastSliderUp > 0) then -- another groupNbr was last
							set L3rdNbr to item lastSliderUp in groupL3rdNbr
							setShot("L3rd", L3rdNbr, "foreground", true)
						else
							setShot("L3rd", 0, "foreground", false)
						end if
					end if
				end if
			end if
			
			if ((groupNbr is not equal to lastButtongroupNbr) and (lastButtongroupNbr > 0)) then -- It's not our slider
				if (item lastButtongroupNbr in groupSliderOn) then -- Ours is up too
					if (lastSliderUp = groupNbr) then -- last slider up was them
						set lastSliderUp to lastButtongroupNbr -- they were last slider so make us last slider
					else
						set L3rdNbr to lastSliderUp in groupL3rdNbr
					end if
					setShot("L3rd", L3rdNbr, "foreground", true)
				else
					if (groupNbr = lastSliderUp) then -- We're not up, and they were last
						set lastSliderUp to 0
						setShot("L3rd", fgNbr, "foreground", true)
					else
						if (lastSliderUp > 0) then
							set L3rdNbr to item lastSliderUp in groupL3rdNbr
							setShot("L3rd", L3rdNbr, "foreground", true)
						end if
					end if
				end if
			end if
		end if
		
		set item groupNbr in groupSliderOn to state
	on error
		if (debugOn) then say "Unexpected error in do slider"
	end try
end doSlider



on bowString()
	tell application "Wirecast-5.0.3"
		set myDoc to last document
		set the transition speed of myDoc to "normal"
		set the active transition popup of myDoc to 3
	end tell
end bowString



on selectCut()
	global debugOn
	
	try
		tell application "Wirecast-5.0.3"
			set myDoc to last document
			set the transition speed of myDoc to "normal"
			set the active transition popup of myDoc to 1
		end tell
	on error
		if (debugOn) then say "Unexpected error in select cut"
	end try
end selectCut



on setSpeed(speed)
	global debugOn
	
	try
		tell application "Wirecast-5.0.3"
			set myDoc to last document
			set the active transition popup of myDoc to 2
			set the transition speed of myDoc to speed
		end tell
	on error
		if (debugOn) then say "Unexpected error in set speed"
	end try
end setSpeed



on processKnob9(knob9)
	global B9t
	global B9b
	global knob9Shot
	global knob9Bg
	global knob9Ov
	global lastShotButton
	
	if (B9t and (not B9b)) then -- Top button only ON (Do a shots)
		if (knob9 is not equal to knob9Shot) then
			setShot("Shot", knob9, "normal", true)
			set knob9Shot to knob9
			set item 1 in lastShotButton to knob9
		end if
	end if
	
	if ((not B9t) and B9b) then -- Bottom button only ON (Do b shots)
		if ((knob9 + 21) is not equal to knob9Shot) then
			set knob9Shot to knob9 + 21
			setShot("Shot", knob9Shot, "normal", true)
			set item 1 in lastShotButton to knob9Shot
		end if
	end if
	
	if (B9t and B9b) then -- Both buttons ON (Do Backgrounds)
		if (knob9 is not equal to knob9Bg) then
			setShot("Bg", knob9, "background", true)
			set knob9Bg to knob9
		end if
	end if
	
	if (not B9t) and (not B9b) then -- Both buttons OFF (Do overlays)
		if (knob9 is not equal to knob9Ov) then
			setShot("Ov", knob9, "overlay", true)
			set knob9Ov to knob9
		end if
	end if
end processKnob9



on saveConfig(groupNbrIndex)
	global sceneNbr
	global groupShotFgNbr -- {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
	global groupL3rdNbr -- {1, 2, 3, 4, 5, 6, 7, 8}
	global groupShotNbr -- {1, 2, 3, 4, 5, 6, 7, 8, 22, 23, 24, 25, 26, 27, 28, 29}
	global groupShotBgNbr -- {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
	
	set fileName to "KANWC" & sceneNbr & "-" & groupNbrIndex & ".config"
	
	try
		set newFile to (((path to documents folder) as string) & fileName) as file specification
		open for access newFile with write permission
		set eof of the newFile to 0
		
		write groupShotFgNbr to newFile starting at eof as list
		write groupL3rdNbr to newFile starting at eof as list
		write groupShotNbr to newFile starting at eof as list
		write groupShotBgNbr to newFile starting at eof as list
		
		close access newFile
		say "Configuration " & groupNbrIndex & " for scene" & sceneNbr & "Saved"
	on error
		say "Save configuration " & groupNbrIndex & "failed"
		close access newFile
	end try
end saveConfig



on loadConfig(groupNbrIndex)
	global sceneNbr
	global groupShotFgNbr -- {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
	global groupL3rdNbr -- {1, 2, 3, 4, 5, 6, 7, 8}
	global groupShotNbr -- {1, 2, 3, 4, 5, 6, 7, 8, 22, 23, 24, 25, 26, 27, 28, 29}
	global groupShotBgNbr -- {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
	
	set fileName to "KANWC" & sceneNbr & "-" & groupNbrIndex & ".config"
	
	try
		set newFile to (((path to documents folder) as string) & fileName) as file specification
		open for access newFile without write permission
		
		set groupShotFgNbr to read newFile as list
		set groupL3rdNbr to read newFile as list
		set groupShotNbr to read newFile as list
		set groupShotBgNbr to read newFile as list
		
		close access newFile
		say "Configuration " & groupNbrIndex & " for scene " & sceneNbr & "Loaded"
	on error
		say "Can not load configuration " & groupNbrIndex
		close access newFile
		set posixPath to POSIX path of newFile as string
		do shell script "rm -rf \"" & posixPath & "\""
	end try
end loadConfig
