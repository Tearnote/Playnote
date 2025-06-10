# BMS chart pipeline

In Playnote, charts are processed through a multi-stage process that can be quite confusing at first. The intention is to sanitize the data early, cache it in a compact format, and process it into the most optimal form for gameplay.

Crossed-out text indicates parts that are planned, but not yet implemented.

## 1. BMS file

A file with `.bms`, `.bme`, `.bml`, etc. extension. The inputs from it are binary contents ~~and the extension (sometimes required to determine playstyle.)~~

## 2. Initial processing

The binary contents are heuristically analyzed for the most likely encoding (there's no fully reliable way to know if a BMS uses Shift-JIS or EUC-KR.) It is then converted from it to UTF-8, and line endings are normalized to `LF`. Outputs are UTF-8 file contents, chart MD5, ~~and file extension~~.

## 3. Parsing

The file contents are processed line by line. Comments are discarded, remaining lines are split into header and channel commands, and each type of command is tokenized into their standard components. The components aren't checked for validity, but obviously nonsensical commands are discarded. Outputs are a stream of header commands and channel commands, interleaved in file order, chart MD5, ~~and file extension~~.

## 4. IR generation

The command stream is consumed by the IR generator, which interprets each individual command type and processes it into a compact binary form. Header names become enum values, slot names become indices. Channel commands are split into individual events. Unknown commands or invalid values are discarded. ~~Control flow is flattened and converted into per-event variable dependencies.~~ Output is an IR structure, containing file metadata and two separate streams of events - header event stream and channel event stream, both of them in file order.

~~The IR can be serialized to a binary blob and cached, to be used instead of the BMS file when a chart is to be played.~~

## 5. Chart building

A chart is built by parsing the IR streams. ~~Random variables are determined, and used to filter out control-flow-dependent events.~~ Header events are used to establish definite metadata, and channel events produce notes ~~and measure timing information~~. Notes are separated into lanes, their vertical positions and timestamps established, and then sorted from earliest. The list of slots actually used by events is enumerated, and a list of requests is built containing all audio, ~~image and video~~ files used by the chart. Finally, metrics are calculated from the finished chart to establish runtime details like note count, loudness, ~~length, BMS feature usage, note density, most common BPM~~. The output is an immutable chart structure, containing metadata, metrics, lanes ~~and scroll speed timeline~~.

~~If the chart doesn't use control flow, then metadata and metrics can be cached to speed up future loading.~~

## 6. Cursor playback

A cursor can be created for a chart, which tracks overall progress, each lane's state and remaining notes, each audio slot's playback progress ~~and player performance~~. Cursors generate audio while progressing a chart's state one sample at a time ~~and interpreting player inputs~~.

Many cursors to a chart can exist at the same time, for example to implement background tasks like preview rendering. ~~Cursors can be serialized to implement replays, instant seeking or online play.~~
