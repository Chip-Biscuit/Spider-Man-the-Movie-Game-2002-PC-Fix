[![DX Version](https://img.shields.io/badge/DirectX-8/9-informational)](#f)
[![Platform](https://img.shields.io/badge/Windows-x86-blue)](#f)
[![Config](https://img.shields.io/badge/Config-INI-success)](#f)
[![Resolution](https://img.shields.io/badge/Resolution-1080p–4K%2B-brightgreen?style=flat)](#resolution)
[![FOV](https://img.shields.io/badge/FOV-Configurable-blue?style=flat)](#fov)
[![Controller](https://img.shields.io/badge/Controller-PS2--style%20XInput-orange?style=flat)](#controller)
[![FMV](https://img.shields.io/badge/FMV-Fullscreen%20Fix-purple?style=flat)](#fmvs)
[![PCGamingWiki – Essential fix](https://img.shields.io/badge/PCGamingWiki-Essential%20fix-0066cc?style=flat&logo=pcgamingwiki&logoColor=white)](https://www.pcgamingwiki.com/wiki/Spider-Man_(2002))

<br><br>
# Spider-Man the Movie Video Game 2002 PC Fix
![sm2002](https://github.com/user-attachments/assets/096f330c-4ca2-4245-bd8b-5eeb871d2bdd)
<br><br>
# Official video guide,
Watch the official fix guide video here on how to install the game and use the fix 
<a href="https://youtu.be/Hb3Nz8OkbBo?si=dtdpuxO7ZkFQN3Cg">
 <img src="https://github.com/user-attachments/assets/3ee16d42-5348-4947-8ce5-7d37d4d26372" alt="Spiderman 2002" width="480" height="360"> </a><br> 
###### <i>Click the image above to watch the video</i>
<br><br>
# Requirements before using fix
For the fix to work properly you must have patched the game up to the **1.3 release**, which also contains major bug fixes. If you have not done so already then you can download the patch from here https://community.pcgamingwiki.com/files/file/791-spider-man-the-movie-patch-13/ 
<br><br>
# Important Information
**Chip** has spent some time reverse engineering an issue where the game was crashing on start up, this has now been traced to the cause and a dinput.dll has now been included into start-up-crash-with-controller-fix.zip in releases to address this so there should be no more start-up crash issues.
<br><br>
# Instructions
Go to releases and download the latest Spider-Man 2002 fix (that is correct for you) of either: 
-    **custom-controller-support-fix-zip**
-    **SM2002Fix-window-vsync-dx8to9.zip**
-    **Spiderman2002Fix.zip**
-    **start-up-crash-with-controller.zip**
<br>
Please bare in mind that if you have the start up crash issue then you can’t use the custom x-input controller addition due to compatibility issues in order to fix that crash problem, so you must use start-up-crash-with-controller to be able to use the games original controller d-input support.
<br>
Once you have downloaded the correct fix for you extract the files into your games main install folder next to the
 **SpiderMan.exe**  file and you are good to go! You can edit the settings you wish to use in the various ini files.
<br><br>

# FMVS

**Chip** has worked very hard and many hours to restore the FMVS to correct size for everyone.
<br>
All fix releases now come with two folders inside 4K and 1080p this is for the FMVS fullscreen fix. To set the size of the FMVS edit the settings inside of the d3d8.ini file. 
<br>
The FMV fullscreen fix has been tested on 1080p, 4K resolutions. This is also tested on the Steam Deck by using the 1080p folder and setting the Resolution in d3d8.ini. 
<br>
4K and 1080p are the only tested Resolutions any others are not guaranteed to work. You will have to try other resolutions and see for yourself if they work properly. The basic rule is: 
<br>
1080p and under use 1080p fix folder <a id="fmvs"></a>
<br>
Anything above 1080p use the 4K fix file
 <br>
You may however need to play around with this
<br>
If you don’t want intro movies to play and get straight into the game then:
<br> 
**Go to path-to-game\movies and delete or rename the following files** (make sure you back them up somewhere else):
<br> 
-    **ACTIVISN.bik**
-    **GrayMatr.bik**
-    **Origin1.bik**
-    **Treyarch.bik**
<br> 
If you are experiencing a crashing issue with the FMVS/Movies, then you may have to rename the folder in your games install location ‘Movies’ to a different name such as ‘Movies Broken’ so then the game plays without the FMVS/movies at all.
<br><br> <a id="resolution"></a>

# Resolution/Aspect Ratio

Choose the resolution you wish to use in the **d3d8.ini** file by writing the Width and Height for your specific resolution. Aspect Ratio is automatically calculated.
<br><br>
# FOV

To choose your FOV edit it in the **d3d8.ini** file There are 4 different options to choose from (1), (2), (3) and (4) with each one zooming out the FOV more each time. It’s advised to not go above (2) if you do not wish to see any objects popping in and out of view in open city levels. If you don’t wish to use any FOV and stick with original, you can just set this to (0).
<br><br> <a id="fov"></a>
# FPS

The default for FPS is (60) you can change it as you wish in the **dxwrapper.ini** file under the option **OverrideRefreshRate**. It is also recommended that you leave both v-sync options on by default.
<br><br> <a id="controller"></a>
# Controller Support (X-Input)

**Chip** has also now spent some time writing a fully custom dinput8.dll which provides x-input support for the game. This will work with Xbox controllers; you can tweak right stick sensitivity in controllersupport.ini. If you don’t have an Xbox controller and use PlayStation, you can use DS4/5 or add the game to Steam as a non Steam game.
<br>
If you want custom controller support use **custom-controller-support-fix.zip.** 
<br>
Custom controller support will only work for those without the crashing on start up issue, if you have the crashing on start up, you must use **start-up-crash-with-controller-fix.zip.**
<br>
The mapping for the custom controller support is as follows: 

<img width="3297" height="1661" alt="Spider-Man_2002_Xbox_Controller_Layout" src="https://github.com/user-attachments/assets/7af8acee-a991-4bcf-8d25-1eef85700da6" />

<img width="1220" height="1617" alt="Spider-Man_2002_Controls_Table" src="https://github.com/user-attachments/assets/a9a75aa7-0e8f-456a-ba31-5a2c8ba02bc3" />


<br>
for a more in depth look at the controls for the game 
https://strategywiki.org/wiki/Spider-Man_(2002)/Controls
<br><br>

# Vote to see the game return via GOG Dreamlist

If you are interested in potentially seeing this game easily available to purchase and use today then go and vote on the games **GOG Dreamlist** to help make this become a reality, you can vote for the game here and write a message about the game if you wish 
https://www.gog.com/dreamlist/game/spider-man-2002 
<br><br>
# Issues/Problems

If you have any** issues**, with the fixes then please go to **Discord** for help linked below. https://discord.gg/eVJ7sQH7Cc
<br>
**Credits**
Credit to Elisha Riedlinger for the base wrapper and ThirteenAG.
Samuel Grossman - xidi used to get controllers working in start-up-crash-with-controller-fix.zip
Brought to you by **Fix Enhancers**
https://fixenhancers.wixsite.com/fix-enhancers
<br><br>
# Team Fix Enhancers:

“Creating compatibility fixes and enhancements for legacy PC games.”
# Chip

- founder
- reverse engineer
- programmer
- developer
  
<img width="250" height="500" alt="my logoo" src="https://github.com/user-attachments/assets/9bb13d3f-0734-4f1d-b68f-14114b13744a" />


# JokerAlex21 

- founder
- admin
- tester 

<img width="250" height="250" alt="YouTube_Logo" src="https://github.com/user-attachments/assets/5c7204ca-4bca-4673-8117-965732e7ee6d" />
