## Purpose

Lets the lister play animated GIFs in place — honoring the file's per-frame delays and loop count — with playback lifecycle tied to the document being open, and renders static GIFs once.

## ADDED Requirements

### Requirement: Animate GIF frames in place

When the open document is an animated GIF, the viewer SHALL advance through the frames in place using the file's declared per-frame delays, without changing the current page.

#### Scenario: Frames advance on declared delays
- **WHEN** an animated GIF is open and the delay for the current frame elapses
- **THEN** the viewer advances to the next frame and repaints the page

#### Scenario: Current page does not change while animating
- **WHEN** a frame advances during playback
- **THEN** the viewer remains on page 1 of the single-page document

### Requirement: Honor the GIF loop count

The viewer SHALL honor the file's declared loop count when repeating playback, and static GIFs SHALL render exactly once without looping.

#### Scenario: Loops the declared number of times
- **WHEN** an animated GIF declares a finite loop count greater than one
- **THEN** the frame sequence repeats for that many loops and then stops

#### Scenario: Static GIF renders once
- **WHEN** the user opens a GIF with no animation
- **THEN** the viewer renders its single frame and performs no playback

### Requirement: Playback lifecycle tied to open/close

GIF playback SHALL stop when the document is closed, the user switches to another file, or the lister window is destroyed; playback SHALL NOT run in the background after the document is gone.

#### Scenario: Playback stops on file switch
- **WHEN** the user switches from an animated GIF to another document
- **THEN** playback stops and no further frames of the GIF are rendered

#### Scenario: Playback stops on close or window destruction
- **WHEN** the animated GIF's lister is closed or its window is destroyed during playback
- **THEN** the playback timer is cancelled and no frame callback fires afterwards