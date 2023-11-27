# Critters

Have a cute creature roam on your Linux desktop,
jumping from window to window.

https://github.com/mnottale/linux-desktop-critters/assets/3638847/38f1e9cf-f139-46d1-84ff-195a7ed96b83

## Dependencies

    wmctrl (package wmctrl)
    xwininfo (package x11-utils)

## Building (tested with QT5)

    qmake critters.pro
    make

## Running

    ./critters

critters will look for helper shell scripts in $PWD, do not run from other
locations

## Using your own animations

Pass on the command line a folder containing your animation frames.

They must be in the format "<animname>_0<framenum>.png".

animname must include the followings:

    walk run jump bite idle sit standing sitting howl

plus a flipped version of each with "_r" suffix.


## Credits

Embeded anim is "animated wolf 2d game sprite" by RobertBrooks.
