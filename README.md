# Spider-Man-the-Movie-Game-(2002)-PC-Fix

![sm2002](https://github.com/user-attachments/assets/096f330c-4ca2-4245-bd8b-5eeb871d2bdd)

# Official video guide

Watch the official fix guide video here on how to install the game and use the fix

<a href="https://youtu.be/Hb3Nz8OkbBo?si=dtdpuxO7ZkFQN3Cg">
  <img src="https://github.com/user-attachments/assets/3ee16d42-5348-4947-8ce5-7d37d4d26372" alt="Spiderman 2002" width="480" height="360">
</a><br>

###### <i>Click the image above to watch the video</i>


# Requirements before using fix
For the fix to work properly you must have patched the game up to the 1.3 release, which also contains major bug fixes. If you have not done so already then you can download the patch from here - https://community.pcgamingwiki.com/files/file/791-spider-man-the-movie-patch-13/ 

# Important Information
We are aware of an issue with the games FMVs/Movies not scaling correctly and currently playing in a small box. This problem is being worked on but is a very complicated issue to resolve so will take time to fix properly. So please do keep an eye on future updates in the releases area for updated downloads of the fix where this issue may be fixed, however, for now please be patient regarding this issue, thank you!

If you are experiencing a crashing issue with the FMVs/Movies then you may have to re name the folder in your games install location ‘Movies’ to a different name such as ‘Movies Broken’ so then the game plays without the movies at all whilst the crashing issue is being looked into.

If you are experiencing crashing on start up issue make sure to download the start-up-crash-with-controller-fix.zip

# Instructions
Go to releases and download the fix that you wish to use for your specific use case, each one is detailed in the releases page. 

# Chip 

I have spent some time reverse engineering an issue where the game was crashing on start up, this has now been traced to the cause which a dinput.dll has now been included into two fixes in the releases to address this so there should be no more start-up crash issues.

I have also now spent some time writing a fully custom dinput8.dll which provides xinput support for the game this will work with xbox controllers, so if you dont have one you can use ds4 windows or add the game to steam as a non steam game or some other method to fake xinput.

the mapping for the custom controller support is as follows: 

<img width="3291" height="1673" alt="Spider-Man_2002_Xbox_Controller_Layout" src="https://github.com/user-attachments/assets/316b24a5-6ef5-42a8-a88a-2a679368c571" /><br>

<img width="567" height="832" alt="Spider-Man_2002_Controls_Table" src="https://github.com/user-attachments/assets/8167413a-53df-4605-8315-569737d9ab52" /><br>

for a more in depth look at the controls for the game : https://strategywiki.org/wiki/Spider-Man_(2002)/Controls

# Resolution
The default for resolution is 1920x1080 you can enter any width/ height you desire in d3d8.ini

# FPS
The default for FPS is (60) you can change it as you wish or fully uncap it with (0) with the FPSLimit option in the d3d8.ini file.

# FOV
The default for FOV is (0) which is the original games FOV. You can choose either 1,2,3 or 4 in the d3d8.ini file and each option will increase the FOV in game.

# Vote to see the game return via GOG Dreamlist
If you are interested in potentially seeing this game easily available to purchase and use today then go and vote on the games GOG Dreamlist to help make this become a reality, you can vote for the game here and write a message about the game if you wish – https://www.gog.com/dreamlist/game/spider-man-2002 

# Issues/Problems
If you have any issues, with the fixes then please go to discord for help linked below. https://discord.gg/eVJ7sQH7Cc

Credits

Credit to Elisha Riedlinger for the base wrapper and ThirteenAG.

Samuel Grossman - xidi used to get controllers working 


Brought to you by Fix Enhancers - https://fixenhancers.wixsite.com/fix-enhancers

# Team fix enhancers:
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
