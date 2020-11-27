# MaterialInstanceConstant Editor
An [UnrealEngine PythonSDK](https://github.com/bl-sdk/PythonSDK) Plugin that allows the easy editing of MaterialInstanceConstant Objects in game.  
Compatible with BL2 and TPS.


## Installation
1. Make sure you have the most recent version of [UnrealEngine PythonSDK](https://github.com/bl-sdk/PythonSDK) installed.
2. Download the newest version of the [MICEditor.zip from here](https://github.com/juso40/MICEditor/releases).
3. Extract the ``miceditor.dll`` from the downloaded ``MICEditor.zip``.
4. Place the ``miceditor.dll`` into your games (either BL2 or TPS) ``Binaries/Win32/Plugins/`` directory. (You might need to create the ``Plugins`` folder)


## Usage
You open/close the Editor using the ``Insert`` Key on your keyboard. As or now you can't rebind the key.  
To update the list of loaded ``MaterialInstanceConstant``s use the ``Update MaterialInstanceConstant List`` button.
(Note: Not all shown Objects are loaded on every map. So when you travel and try to edit an old Object it might not work.)  

You can either double click the values to edit them using Keyboard or drag and slide them. Some buttons will open up a new window, there can always be only one child window, so make sure to close it when you don't need it anymore.
To export the Mod open up the ``Export To File`` tab and type chose a name for your exported files name. After pressing the ``Export`` button the file should be located in your games ``Binaries/`` directory. 

If you are trying to edit some objects in the game world, i suggest to look directly at it, then using the ``freezeat here`` console command. 
This will stop your camera from moving. To unfreeze the camera simply use the ``freezeat`` console command.


-------------------------------
### Contact me
- Discord `juso#6352`