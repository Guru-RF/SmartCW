# keysim: the keyer and decoder on the host

Compiles `src/engine.cpp`, `src/fist.cpp` and `src/morse.cpp` natively with
stubbed Arduino, GPIO and timer headers, drives the paddle inputs from a
script and checks what the decoder makes of hand-keyed fists: dah ratios
from 2.2 to 4.2, tight and swinging spacing, slow and fast hands, a change
of operator mid-stream, runs of dits and dahs, a bug, paddles with hesitant
spacing, and a fist seeded from `config.txt`.

    cd tools/keysim
    g++ -std=c++17 -O1 -I. -I../../src sim.cpp -o sim && ./sim

Exit status 1 when any test decodes with errors; the per-test lines show the
learned fist and the decoded text. Cold starts miss a first letter or two by
design (the fist is learned from the letters themselves).
