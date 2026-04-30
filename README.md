Hi, I am happy to see you here :D

This is the super cool bombermanjj project created by Jans and Justs , hence the -jj at the end of bomberman.
This is the final exam in one of our courses in University of Latvia.
Jans is the main developer of this with 60% of the work done by him, he implemented the very basics of the server and made all the protocols
as well as many other game features. Justs did 40% of the work by mostly contributing for adding the logic for placing bombs, protocols that are
related to them as well as everything behind powerups.

Now to the important stuff, if you want to play this game all you have to do is either clone this repo
and run it locally or you can access a server which already has it. 

When you have the access you must navigate to the root of the project, in this case the bombermanjj.
When you are inside the root direcotry, you can run the "make" command, it will create two new executable files 
which can be run in this order. 

First you do ./server which will configure a running server and then players who want to join in can do ./client {version} {nickname}
Note: you can write whatever you want in the version field
the game waits for players but when all of them press the letter "r" (ready) then the game will start.

You can navigate using the arrow keys and space to lay bombs.
To disconnect from the server you can use the Esc button.
Have fun ;)
