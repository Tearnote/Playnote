# Departures from tradition

Playnote intentionally ignores certain parts of the BMS specification and de-facto standard BMS player practice. These will be listed below.

## Timing windows

The `#RANK` command is not respected; instead, every chart has the same timing window. BMS timings are notoriously wide, since the majority of charts opt for EASY judge. Some don't, however, which creates a jarring experience. Other rhythm games have shown that stable timing windows are ultimately the better idea, and they help with making sure one's "earthpower" level doesn't become too unbalanced from their accuracy.

## Gauges

The `#TOTAL` command is not respected; instead, every chart has the same gauge restoration speed. In other BMS players the TOTAL value affects the ratio of gauge restoration vs drain, which makes it easier or harder to survive at the same player performance level. There is no sensible reason why this should be different per chart, and various gauges already exist to adjust the difficulty of staying alive.

## Difficulty

The `#PLAYLEVEL` command ("number" difficulty) is ignored and not surfaced in the UI; `#DIFFICULTY` (the B/N/H/A/I "name" difficulty) is still shown as background color. As the difficulty number is set by the chart creator with no clear guidelines, it's not very useful in practice. There's also no reliable indication if the chart creator is referring to normal or insane scale. Tables are supported, which are the typical way of achieving consistent difficulty numbering in BMS. For loose charts, players are advised to consult average and peak note density.
