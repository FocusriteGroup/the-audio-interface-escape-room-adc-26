# Spoilers: the answers

This folder holds the correct versions of the processing files that each exercise
puts in `exercise/`. Comparing an exercise file with its copy
here shows you the bug straight away.

Treat it like the answer sheet in an escape room: it's here so the instructors can
run the solutions and help anyone who falls behind, not as a shortcut. You'll get
far more out of the workshop by measuring, narrowing down the faulty stage and
fixing it yourself. If you're stuck, ask for a hint first.

Editing these files doesn't change an exercise build. Exercises compile the copies
in `exercise/`, which the runner fills from `audio-core/.exercises/`; only the
baseline app and solution mode use this folder.
