## Purpose

Defines how the viewer plays back animated GIF files: frame decoding, per-frame timing, looping, and single-page treatment. Static GIFs are covered by the same capability. Applies to both the Win32 viewer on Windows and the Qt viewer on Linux with identical behavior.

## ADDED Requirements

### Requirement: Treat an animated GIF as a single animating page

When a GIF with more than one frame is opened, the viewer SHALL present it as exactly one page that animates in place, rather than as a sequence of individual frame pages. The page indicator, navigation, zoom/fit, rotation, and print commands SHALL operate on that single animated page.

#### Scenario: Open an animated GIF
- **WHEN** a multi-frame GIF is opened
- **THEN** the viewer reports one page and displays the animation advancing in the viewport

#### Scenario: No navigation between frames
- **WHEN** an animated GIF is open and a next-page or previous-page command is issued
- **THEN** the command has no effect within the GIF itself (the page is both the first and last page)

### Requirement: Advance frames on the file's own timing

The viewer SHALL advance an animated GIF's frames on a timer whose interval matches the current frame's declared delay, re-arming after each frame from the next frame's declared delay.

#### Scenario: Frame delays differ
- **WHEN** a GIF declares a 50 ms delay for frame 1 and a 300 ms delay for frame 2
- **THEN** frame 2 appears 50 ms after frame 1 and frame 3 appears 300 ms after frame 2

#### Scenario: Repaint in place on frame change
- **WHEN** the timer fires and the frame changes
- **THEN** the viewport repaints the new frame in the same page area without a page transition

### Requirement: Respect the file's loop semantics

The viewer SHALL honor the GIF's declared loop count: play once for a no-loop GIF, loop the declared number of times for a finite-loop GIF, and loop indefinitely for an infinite-loop GIF. A finite-loop GIF that has finished its declared loops SHALL remain on its last frame and stop the timer.

#### Scenario: Infinite-loop GIF
- **WHEN** an animated GIF declares infinite looping
- **THEN** the animation loops on the timer until the document is closed or the viewer window is destroyed

#### Scenario: Finite-loop GIF completes
- **WHEN** a GIF declares it loops twice and both loops have completed
- **THEN** the animation stops on the final frame and no further timer ticks occur

### Requirement: Render a static GIF without animating

When a GIF contains a single frame, the viewer SHALL render that frame once and SHALL NOT start a playback timer.

#### Scenario: Single-frame GIF
- **WHEN** a single-frame GIF is opened
- **THEN** the frame is displayed once and no animation runs

### Requirement: End playback with the document

Playback SHALL stop when the document is closed, when a different file is loaded into the lister, or when the viewer window is destroyed, and SHALL NOT leak timers or continue repainting afterwards.

#### Scenario: Close the document during playback
- **WHEN** an animated GIF is playing and the document is closed or replaced
- **THEN** the playback timer is stopped and no further frames are decoded or painted

#### Scenario: Destroy the viewer during playback
- **WHEN** the lister window showing an animated GIF is destroyed
- **THEN** the playback timer is torn down with the window

### Requirement: Same behavior on both platforms

The GNU/Linux and Windows viewers SHALL exhibit the same animation behavior (frame timing, looping, single-page semantics); only the timer mechanism backing the behavior may differ.

#### Scenario: Identical playback across platforms
- **WHEN** the same animated GIF is opened on Windows and on Linux
- **THEN** the frame sequence, timing, and looping behavior of the two viewers match