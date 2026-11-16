Put your own music here: one file per JoJo character, named after him, for example

  jotaro.mp3  dio.mp3  josuke.mp3  johnny.mp3  joseph.mp3  jolyne.mp3  giorno.mp3

and pillar_men.mp3, which plays in the Pillar Men encounter and in all three of their fights.
The names are the ones the game uses for the characters (kira, pucci, weather, vanillaice, ...).
.mp3, .ogg, .m4a and .wav all work.

Each track repeats from its start. To repeat from another point, add a text file with the same
name and .loop holding the seconds, for example jotaro.loop containing 12.5

DIO's time stop has two sounds of its own, which go in the OTHER folder, files\extra-audio\se :
  the_world.mp3          starts when the inverted circle spreads from DIO; the battle waits for it to finish
                         (up to 6 seconds after the circle)
  the_world_resume.mp3   starts after "Time has begun to move again.", as the colour comes back

Then run BUILD-SD-CARD again: it redoes only the music (or the sounds) and the game file.
A character with no file here keeps the usual music; with no sound files the time stop is silent.
