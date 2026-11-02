Crimson Desert co-op test kit (game build 1.0.0.3479 only)

INSTALL (both players)
1. Close the game.
2. Copy winmm.dll and cdmp.asi into:
   <Steam library>\steamapps\common\Crimson Desert\bin64
3. Copy ONE config there and rename it to cdmp.ini:
   - the host:  cdmp.ini.host  -> cdmp.ini
   - the guest: cdmp.ini.guest -> cdmp.ini
4. Open cdmp.ini and set peer_steamid= to the OTHER player's SteamID64
   (17 digits; Steam > profile > Account details, or steamid.io).
   You must be Steam friends.

PLAY
1. Both start the game and load your own saves.
2. Walk somewhere in the open. The host's world is the real one; the guest
   appears in it as a copy, and vice versa.
3. F8 shows the co-op panel (connection status).

WHAT WORKS / WHAT DOESN'T (yet)
- Works: seeing each other walk, run, fight, climb.
- Horse riding: the other player floats where the horse would be; on
  dismount their copy reloads (a short blink).
- Not yet: shared enemies, XP, money, loot.

IF SOMETHING GOES WRONG
Send bin64\cdmp.log (and the newest .dmp from
%LOCALAPPDATA%\Pearl Abyss\DumpCache\reports if the game crashed).
To uninstall, delete winmm.dll, cdmp.asi and cdmp.ini from bin64.
