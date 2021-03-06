
    Welcome to Dingux-TI99

Original Author of TI99/Sim

  Marc Rousseau  (see http://www.mrousseau.org/programs/ti99sim/)

Author of the Gp2x, PSP and Dingoo port versions

  Ludovic.Jacomme also known as Zx-81 (see http://zx81.zx81.free.fr)


1. INTRODUCTION
   ------------

  TI99Sim is a very good emulator of Texas instruments TI99 home computer
  running on Windows and Unix. The emulator faithfully imitates the TI99/4
  model (see http://www.mrousseau.org/programs/ti99sim/)

  Dingux-Ti99 is a port on Dingoo of the version 0.1.0 of TI99Sim.

  This package is under GPL Copyright, read COPYING file for
  more information about it.


2. INSTALLATION
   ------------

  Unzip the zip file, and copy the content of the directory game to your
  SD card.

  Put your cartridge files on "cartridge" sub-directory. 

  For any comments or questions on this version, please visit 
  http://zx81.zx81.free.fr or http://www.gp32x.com/


3. CONTROL
   ------------

  In the TI99 emulator window, there are three different mapping 
  (standard, left trigger, and right Trigger mappings). 
  You can toggle between while playing inside the emulator using 
  the two Dingoo trigger keys.

    -------------------------------------
    Dingoo        TI99            (standard)
  
    Y          2
    A          ENTER
    X          1
    B          Joystick Fire
    Up         Up
    Down       Down
    Left       Left 
    Right      Right

    -------------------------------------
    Dingoo        TI99   (left trigger)
  
    Y          Space
    A          ENTER
    X          3
    B          Joystick Fire
    Up         Up
    Down       Down
    Left       Left 
    Right      Right

    -------------------------------------
    Dingoo        TI99   (right trigger)
  
    Y          Space
    A          ENTER
    X          4
    B          Joystick Fire
    Up         Up
    Down       Down
    Left       Left 
    Right      Right
  
  
  Press Start/Menu   to enter in emulator main menu.
  Press Select       open/close the On-Screen keyboard
  
  In the main menu
  
  X      Go Up directory
  B      Valid
  A      Valid
  Y      Go Back to the emulator window
  
  The On-Screen Keyboard of "Danzel" and "Jeff Chen"
  
  Use digital pad to choose one of the 9 squares, and
  use X, Y, A, B to choose one of the 4 letters of 
  the highlighted square.
  
  Use LTrigger and RTrigger to see other 9 squares
  figures.

4. LOADING CARTRIDGE FILES
   ------------
  
  If you want to load cartridge image in your emulator, you have to put your 
  cartridge file (with .zip, or .ctg file extension) on your Dingoo SD card in 
  the 'cartridge' directory.
  
  Then, while inside the emulator, just press Menu to enter in the
  emulator main menu, and then using the file selector choose one cartridge file
  to load in your emulator. Back to the emulator window, the cartridge should stard
  automatically.
  
  You can use the virtual keyboard in the file requester menu to choose the
  first letter of the game you search (it might be useful when you have tons of
  games in the same folder). Entering several time the same letter let you
  choose sequentially files beginning with the given letter. You can use the Run
  key of the virtual keyboard to launch the cartridge.
  
  You may use the Trigger key to swap between the two virtual keyboard panels
  (numbers & letters)


4. LOADING KEY MAPPING FILES
   ------------

  For given games, the default keyboard mapping between Dingoo Keys and TI99 keys,
  is not suitable, and the game can't be played on Dingux-TI99.

  To overcome the issue, you can write your own mapping file. Using notepad for
  example you can edit a file with the .kbd extension and put it in the kbd 
  directory.

  For the exact syntax of those mapping files, have a look on sample files already
  presents in the kbd directory (default.kbd etc ...).

  After writting such keyboard mapping file, you can load them using 
  the main menu inside the emulator.

  If the keyboard filename is the same as the cartridge file (.ctg)
  then when you load this tape file, the corresponding keyboard file is 
  automatically loaded !

  You can now use the Keyboard menu and edit, load and save your
  keyboard mapping files inside the emulator. The Save option save the .kbd
  file in the kbd directory using the "Game Name" as filename. The game name
  is displayed on the right corner in the emulator menu.


6. CHEAT CODE (.CHT)
----------

  You can use cheat codes with Dingux-Ti99 You can add your own cheat codes in
  the cheat.txt file and then import them in the cheat menu.

  All cheat codes you have specified for a game can be save in a CHT file 
  in 'cht' folder.  Those cheat codes would then be automatically loaded when
  you start the game.

  The CHT file format is the following :
  #
  # Enable, Address, Value, Comment
  #
  1,36f,3,Cheat comment
  
  Using the Cheat menu you can search for modified bytes in RAM between
  current time and the last time you saved the RAM. It might be very usefull to
  find "poke" address by yourself, monitoring for example life numbers.

  To find a new "poke address" you can proceed as follow :

  Let's say you're playing Moon patrol and you want to find the memory address
  where "number lives" is stored.

  . Start a new game in moon patrol
  . Enter in the cheat menu. 
  . Choose Save Ram to save initial state of the memory. 
  . Specify the number of lives you want to find in
  "Scan Old Value" field.
  (for Moon patrol the initial lives number is 4)
  . Go back to the game and loose a life.
  . Enter in the cheat menu. 
  . Specify the number of lives you want to find in
    "Scan New Value" field.
    (for Moon Patrol the lives number is now 3)
  . In Add Cheat you may have several matching Addresses
    (for Moon Patrol you have only one)
  . Specify the Poke value you want and add new cheats 
    with this address / value.
  . Try them one by one to identify what is the good
    one, while restarting new games and see if the 
    life number is 4 or 6. You will see that the good
    address is 83A2. You can delete all others.

  The cheat is now activated in the cheat list and you can save it using the
  "Save cheat" menu.

  Let's enjoy Moon Patrol with infinite life !!

7. COMMENTS
------------

  You can write your own comments for games using the "Comment" menu.  The
  first line of your comments would then be displayed in the file requester menu
  while selecting the given file name (rom, keyboard, settings).


8. SETTINGS
------------

  You can modify several settings value in the settings menu of this emulator.
  The following parameters are available :

  Sound enable : 
    enable or disable the sound

  Speed limiter :
    limit the speed to a given fps value

  Skip frame : 
    to skip frame and increase emulator speed

  Display fps : 
    display real time fps value 

  Render mode : 
    many render modes are available with different geometry that should
    covered all games requirements

  Clock frequency : 
    Clock frequency, by default the value is set
    to 200Mhz, and should be enough for most of all
    games.


9. JOYSTICK SETTINGS
------------

  You can modify several joystick settings value in the settings menu of this
  emulator. The following parameters are available :

  Swap Analog/Cursor : 
    swap key mapping between analog pad and 
    digital pad 

  Auto fire period : 
    auto fire period

  Auto fire mode : 
    auto fire mode active or not

10. COMPILATION
   ------------

  It has been developped under Linux FC9 using gcc with DINGUX SDK. 
  All tests have been done using a Dingoo with Dingux installed
  To rebuild the homebrew run the Makefile in the src archive.

  Enjoy,

            Zx
