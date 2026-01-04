# Copilot Instructions for Bandicam (FFusion Recording)

## Project Overview

**Bandicam** is a cross-platform Qt6 + FFmpeg screen recording application with multi-source capture (screen, camera, system audio, microphone), real-time OpenGL preview, and event-driven architecture. The codebase is entirely in **C++17** with QML interfaces and uses a modular pipeline for capture → demux → decode → filter → encode → mux workflows.

## Architecture & Data Flow

### Core Pipeline

```
QML Signals → QmlBridge → EventFactoryManager → FFEventQueue
          ↓
    FFEventLoop (event consumer)
          ↓
    (SOURCE events) Demuxer & Decoder threads
          ↓
    (PROCESS events) Filter & Encoder threads
          ↓
    FFMuxerThread → MP4 output + progress events
          ↓
    FFVEncoderThread → OpenGL preview (FFGLItem)
```

### Key Architectural Patterns

**Singleton Contexts**:

- `FFRecorder` is the central singleton holding all resource ownership (demuxers, decoders, encoders, queues, threads)
- Access via `FFRecorder::getInstance()` and call `initialize()` before use
- See: `recorder/ffrecorder.h` for all getter methods

**Event-Driven Control**:

- `FFEvent` base class (polymorphic) dispatched via `FFEventQueue::enqueue()`
- Three categories: `SOURCE` (open/close inputs), `PROCESS` (frame processing), `CONTROL` (pause/resume/source-change)
- `EventFactoryManager` creates typed events via factory pattern
- `FFEventLoop` runs in dedicated thread consuming queue indefinitely

**Thread Isolation**:

- Each stage (demuxer, decoder, filter, encoder, muxer) has its own `FFThread` subclass
- Threads share data via thread-safe `FFBoundedQueue` (packet/frame queues)
- `std::mutex` + `std::condition_variable` for synchronization; avoid Qt signals across thread boundaries for hot paths

**Trait-Based Queues**:

- `FFBoundedQueue<T, Traits>` template (in `queue/ffboundedqueue.h`) uses `Traits` classes for memory management
- Examples: `FFPacketTraits` (av_mallocz/av_freep), `AVFrameTraits` (av_frame_alloc/av_frame_unref)
- Supports blocking (`dequeue`) and non-blocking (`tryDequeue`) operations
- Max size default: 30 items; critical for preventing memory bloat during capture

## Directory Map

| Directory              | Purpose                                  | Key Files                                             |
| ---------------------- | ---------------------------------------- | ----------------------------------------------------- |
| `recorder/`            | Central context & initialization         | `ffrecorder.h`, `ffrecorder_p.cpp`                    |
| `event/`               | Event definitions, factories, loop       | `ffevent.h`, `eventfactorymanager.h`, `ffeventloop.h` |
| `demuxer/`, `decoder/` | FFmpeg input handling                    | `demuxer.h`, `ffadecoder.h`, `ffvdecoder.h`           |
| `encoder/`             | H.264 codec selection (hardware→libx264) | `ffvencoder.cpp` (smart encoder selection)            |
| `filter/`              | Audio mixing & video effects             | `ffafilter.h`, `ffvfilter.h`                          |
| `muxer/`               | MP4 output assembly                      | `ffmuxer.h`                                           |
| `queue/`               | Thread-safe bounded queues               | `ffboundedqueue.h`, `ffvpacketqueue.h`, etc.          |
| `thread/`              | Worker threads for each stage            | `ffdemuxerthread.h`, `ffvencoderthread.h`, etc.       |
| `opengl/`              | Real-time YUV→RGB preview                | `ffglitem.h`, `ffglrenderer.h`                        |
| `qml/`                 | UI & interaction layer                   | `Main.qml`, `settingpage.qml`                         |

## Development Patterns

### Adding New Functionality

**1. New Event Type**:

- Extend `EventCategory` enum in `eventcategory.h`
- Create event class inheriting `FFEvent` (e.g., `ffsourcecustomizeevent.h`)
- Register factory in `EventFactoryManager::EventFactoryManager()` constructor

**2. New Codec/Encoder**:

- Add candidate to `candidates[]` array in `FFVEncoder::initVideo()` (line ~195-200)
- Encoder selection pattern: NVENC → AMD AMF → Intel QSV → libx264 (fallback)
- Hardware encoders use `thread_count=1`; software uses frame-level threading

**3. New Input Source**:

- Extend `demuxerType` enum
- Implement URL parsing in `Demuxer::init()`
- Register demuxer thread in `FFRecorder::initialize()`

### Memory Management

- **Manual allocation in FFmpeg paths**: Use `av_malloc`, `av_free` consistently
- **Qt ownership**: Pass `nullptr` to QObject parents for non-Qt-owned resources
- **Queue cleanup**: Always call `FFBoundedQueue::close()` in destructors to wake waiting threads
- **Frame lifetime**: Frames are ref-counted by FFmpeg; use `av_frame_move_ref` when transferring between queues

### Thread Safety

- **Mutex scopes**: Use `std::lock_guard<std::mutex>` with RAII; avoid nested locks (deadlock risk)
- **Atomic flags**: Use `std::atomic<bool>` for stop/pause signals (memory_order_acquire/release)
- **No Qt signals in hot paths**: Event queues and encode loops use std::condition_variable, not Qt slots
- **PTS clock**: Guarded by mutex in encoder; critical for pause/resume consistency

### Logging & Debugging

- Qt messages logged to `bandicam.log` via `qtLogHandler` in `main.cpp`
- FFmpeg errors printed via `printError()` helper (maps error codes to strings)
- Performance profiling code in `ffvencoder.cpp` commented out but shows pattern for timing analysis

## Build & Test Workflow

**Prerequisites**:

- Qt 6.5+ (Quick, QML, Multimedia, Widgets, Gui modules)
- FFmpeg Windows precompiled (in `3rdparty/ffmpeg-amf/include` & `lib/`)
- MSVC or MinGW C++17 compiler
- CMake ≥ 3.16, Ninja (optional but recommended)

**Build Commands**:

```powershell
cd d:\Qtprogram\bandicam
cmake -B build -S . -G "Ninja" -DCMAKE_PREFIX_PATH="C:\Qt\6.8.0\msvc2019_64"
cmake --build build --config Release
# FFmpeg DLLs auto-copied to build/appbandicam_autogen/.../
.\build\appbandicam.exe
```

**Common Issues**:

- Missing `dshow.h`: Ensure FFmpeg headers include `libavdevice`
- Encoder selection fails: Check FFmpeg build includes libx264, NVENC/AMF support
- Pause/resume hangs: Verify PTS clock is reset in `FFVEncoder::resetPtsClock()`

## Code Style & Conventions

- **Naming**: `FF*` prefix for FFmpeg wrappers (`FFVEncoder`, `FFDemuxer`), classes in PascalCase, private members `m_` or in `_p.h`
- **Comments**: Inline Chinese comments explain non-obvious intent; functions documented with purpose
- **Error handling**: Return `int` (0 success, <0 error) or `nullptr`; use `av_strerror()` for FFmpeg errors
- **Resource cleanup**: Destructor calls `close()` method to ensure queue/codec/frame cleanup before deletion

## Performance Considerations

- **Encoder tuning** (`ffvencoder.cpp`):
  - Hardware encoders: GOP=60, single-threaded, CBR mode
  - Software encoders: GOP=30, frame-level parallelism (up to 16 threads), ultrafast preset
  - Bitrate adaptive: 1Mbps (SD) → 4Mbps (1080p+)
- **Queue buffer sizes**: Default 30 items; adjust if memory is constrained or latency critical
- **Preview rendering**: Three-texture YUV upload in `FFGLRenderer`; GLSL shader conversion to RGB
- **Pause optimization**: Stops frame processing immediately; resume adjusts PTS offset instead of re-encoding

## Muxer Optimization Roadmap

**Current Challenges** (vs FFmpeg official `ffmpeg_mux.c`):

- ❌ No DTS monotonicity guarantee → may cause container format warnings
- ❌ Invalid timestamps dropped instead of compensated → progress bar jumps
- ❌ No BSF (bitstream filter) chain support → relies on encoder output correctness
- ✅ Adaptive sync algorithm (innovation) → more lightweight than global sync queue

**Phase 1 Improvements** (High Priority):

1. Implement `mux_fixup_ts()` for DTS monotonicity (prevent playback issues)
2. Add `compensate_invalid_ts()` instead of discarding (prevent frame drops)
3. Track per-stream state with `stream_states` map

**Phase 2 Improvements** (Medium Priority):

1. File size limit detection (prevent disk overflow)
2. Mux statistics collection (video/audio size, frame counts)
3. Enhanced error recovery (max 10 consecutive errors before exit)

**Phase 3 Improvements** (Low Priority):

1. Optional BSF chain support for H.264 mp4toannexb conversion
2. Global sync queue if multi-file synchronization needed

**References**: See `MUXER_OPTIMIZATION_ANALYSIS.md`, `MUXER_COMPARISON_SUMMARY.md`, `MUXER_IMPLEMENTATION_GUIDE.md` for detailed analysis and code examples.

## TODO & Known Limitations

- [ ] Settings page UI not implemented (skeleton exists)
- [ ] Audio/video filter modules stubbed (filter threads created but no DSP)
- [ ] Custom output path & file naming UI missing
- [ ] Monitor UI (volume, framerate overlays) not shown
- [ ] Multi-camera fallback if primary fails
- [ ] Frame drop detection & reporting
- [ ] **Muxer Phase 1**: DTS monotonicity guarantee + timestamp compensation
- [ ] **Muxer Phase 2**: File size limits + statistics collection
- [ ] **Muxer Phase 3**: BSF chain + advanced sync (if needed)

---

**Last Updated**: November 2025 | **Reference**: `README.md`, `main.cpp`, `recorder/ffrecorder.h`, `encoder/ffvencoder.cpp`, `queue/ffboundedqueue.h`, `muxer/ffmuxer.h`, `thread/ffmuxerthread.h`
