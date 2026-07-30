# ReziAudio architecture

## Scope

ReziAudio is the successor of the legacy `ComponentAudioSource` and
`ComponentAudioListener` path. Both systems coexist during migration. The old
components keep their serialized numeric type IDs; ReziAudio component IDs were
appended after them.

The backend is platform-neutral C++ and only depends on miniaudio and
MathGeoLib value types. The engine repository still has a Windows-only
top-level CMake guard, but ReziAudio itself contains no Win32, WASAPI or
DirectSound calls. Removing that project-wide guard later is sufficient for the
audio foundation to use ALSA/PulseAudio/JACK, CoreAudio, AAudio/OpenSL ES or
WebAudio through miniaudio.

## Hazel analysis

The analyzed Hazel checkout is marked `Unlicensed`. Its files must therefore
not be copied into this repository without separate permission from its
copyright owner. ReziAudio uses independently written code and only adopts
architectural ideas.

Hazel's useful audio boundaries are:

1. `MiniAudioEngine` owns the device, update thread, listeners, resource and
   source managers.
2. `ResourceManager` separates decoded/streamed data from playing voices.
3. `SourceManager` owns voice lifetime and DSP graph attachment.
4. `AudioPlayback` is a safe game-facing control object.
5. `AudioEventsManager` and command assets separate game intent from files.
6. `SoundGraph::Prototype` stores graph topology independently from instances.
7. `SoundGraphSource` compiles a prototype into a real-time playback instance.
8. `AudioComponent` bridges scene entities to event playback.
9. DSP, spatialization and reverb are below the event and component layers.

The most important lesson is that an audio clip, a sound definition, a playing
voice and an entity component are four different objects. The legacy EGE
implementation stores one `ma_sound` on `ResourceAudio`, which makes the asset
and the playing voice the same object. ReziAudio never does this.

### Technical portability assessment

| Hazel file or group | Technical effort | Decision |
|---|---:|---|
| `DSP/Components/allpass.*`, `comb.*`, `denormals.h`, `tuning.h` | 0–small | Technically portable and mostly public-domain Freeverb code, but not copied. Use miniaudio nodes first; import only with explicit provenance later. |
| `Buffer/CircularBuffer.h` | small | Structure is portable, implementation is not suitable as the main real-time queue because it is not a complete bounded SPSC abstraction. Reimplement when the audio command thread lands. |
| `Editor/OrderedVector.*` | small | Not audio-specific and contains locking/API issues. ReziAudio graph serialization uses ordinary stable vectors instead. |
| `DSP/Filters/*` | small–medium | Coupled to Hazel's miniaudio wrapper. Prefer native miniaudio low/high-pass nodes. |
| `DSP/Reverb/*` | medium | Freeverb core is portable; Hazel wrapper and allocator are not. Planned as a ReziAudio DSP node, not a direct copy. |
| `AudioCallback.*`, `VFS.*`, `AudioReader.h` | medium | Bound to Hazel streams, CHOC buffers and miniaudio wrapper. EGE already has a PhysFS `ma_vfs`; adapt the interface, do not copy. |
| `Audio.h`, `AudioEngine.*`, `SourceManager.*`, `ResourceManager.*` | large | Strong Hazel Application/Asset/Scene/thread coupling. Architectural reference only. |
| `Sound.*`, `SoundObject.*`, `AudioPlayback.*` | large | Useful contracts, but depend on Hazel Ref/Asset/Timestep/entity registries. ReziAudio equivalents are independently implemented. |
| `AudioEvents/*`, `AudioEventsManager.*`, `EntityIDMaps.*` | large | Hazel UUID, YAML, asset and editor coupling. ReziAudio uses EGE UUID/Config and generation-safe handles. |
| `SoundGraph/*` | very large | Depends on CHOC values/FIFO, farbot, Hazel reflection, serialization, cache and graph editor. Retain the prototype/instance split, replace the implementation. |
| `Editor/AudioEventsEditor.*`, Hazel node graph editor | very large | Editor-framework-specific. The future UI will use the already integrated `thedmd/imgui-node-editor`. |
| `Hazel-ScriptCore/Audio/Audio.cs` | none as code | C# cannot be reused by AngelScript. Its high-level event-oriented style informs the scripting contract. |

## ReziAudio layers

```text
AngelScript / gameplay / editor preview
                 |
      ReziAudioEmitter / Listener
                 |
       Event + graph instance layer
                 |
      ReziAudio::System (safe handles)
                 |
          IAudioBackend contract
                 |
       MiniaudioBackend + bus graph
                 |
       miniaudio device/resource/VFS
```

### Device and backend

`IAudioBackend` is the only layer that knows how a voice is represented. It
accepts immutable create information and live setting/transform updates.
`MiniaudioBackend` borrows the `ma_engine` already owned by `ModuleAudio`.
Consequently legacy and ReziAudio playback share one device during migration.

Every playback has an index plus generation. Destroying and reusing a slot
changes the generation, so a stale script/component handle cannot control a new
sound accidentally.

The initial bus graph is:

```text
Master
├── Music
├── Sound Effects
├── Ambience
└── UI
```

Bus volume is independent of voice volume. Future user buses will be asset
data, not hard-coded enum entries.

### Asset types

The graph-facing asset model is intentionally separate from existing
`ResourceAudio`:

| Asset | Proposed extension | Responsibility |
|---|---|---|
| Audio Clip | `.reziclip.json` | Source UUID/path, import settings, stream/preload, channel/sample metadata. |
| Sound Event | `.rezievent.json` | Optional future alias for a graph reference plus a registered event name, defaults, concurrency and cooldown policy. |
| Sound Graph | `.rezigraph.json` | Nodes, typed pins, links, parameters and editor positions. |
| Mixer | `.rezimixer.json` | User buses, sends, effects, snapshots and exposed controls. |
| Attenuation | `.reziattenuation.json` | Reusable distance/cone/doppler/air-absorption settings. |
| Parameter Collection | `.reziparams.json` | Global bool/int/float/vector/color parameters. |

`ClipAsset`, `SoundGraphAsset`, `GraphNode`, `GraphPin`, `GraphLink` and
`NamedParameter` are already defined as data-only foundation types.

Graph pins support flow, bool, integer, float, Vector2, Vector3, Color, audio
buffer and audio clip values. Adding a custom value type belongs in a central
type descriptor registry; individual node classes must not add editor-specific
variant branches.

### Graph compiler and real-time rules

The stored graph is never evaluated directly on the device callback:

1. Validate types, required pins, cycles and output node count.
2. Topologically sort DSP nodes and allocate all scratch buffers.
3. Convert node IDs and links to compact indices.
4. Produce an immutable compiled program and diagnostic list.
5. Atomically swap the compiled program between callback blocks.

The callback performs no allocation, file I/O, locks, logging, reflection or
asset lookup. UI and script commands enter through a bounded SPSC command
queue. Meters and completion notifications return through a second queue.

Initial node families:

- input: Wave Player, Random Wave, Parameter, Trigger;
- routing: Mixer, Switch, Sequence, Random, Blend;
- control: Delay Trigger, Envelope, Loop, Cooldown;
- DSP: Gain, Pan, Pitch, Low Pass, High Pass, Delay, Reverb;
- spatial: Spatialize, Doppler, Attenuation, Cone;
- output: Event Output and Bus Send.

### Scene components

`ComponentReziAudioEmitter` owns configuration plus a safe playback handle. It
does not own decoded audio. Each emitter can play the same clip independently,
updates settings and 3D transform live, and destroys its voice on disable,
scene stop or destruction.

`ComponentReziAudioListener` supplies position, forward, up and velocity. The
first active ReziAudio listener is authoritative for now. Multiple miniaudio
listeners and split-screen routing can be added without changing emitter data.

Legacy `AudioSource` and `AudioListener` remain loadable and scriptable. A later
migration command can convert them explicitly; scene loading never silently
changes component semantics.

### AngelScript contract

The scripting layer exposes the new typed components now and will expose event
intent and safe playback controls as event assets land. It never exposes a
`ma_sound` pointer:

```angelscript
class ReziAudioPlayback
{
    bool get_valid() const;
    AudioPlaybackState get_state() const;
    void Pause();
    void Resume();
    void Stop(float fadeSeconds = 0.0f);
    void SetFloat(const string &in name, float value);
    void SetBool(const string &in name, bool value);
    void SetVector3(const string &in name, const Vector3 &in value);
}

class ReziAudioEmitter : Component
{
    AudioEvent@ event;
    float volume;
    float pitch;
    bool spatial;
    ReziAudioPlayback Play();
    ReziAudioPlayback PlayOneShot(AudioEvent@ event);
    void StopAll(float fadeSeconds = 0.0f);
}

namespace ReziAudio
{
    ReziAudioPlayback PostEvent(AudioEvent@ event);
    ReziAudioPlayback PostEvent(
        AudioEvent@ event, GameObject@ object);
    void SetGlobalFloat(const string &in name, float value);
    void SetBusVolume(const string &in bus, float value);
}
```

AngelScript wrappers store only the generation-safe handle and resolve it on
each call. Scene/project changes invalidate handles as part of system shutdown.
The existing script API generator must emit these declarations into
`as.predefined`.

The implemented first slice supports typed
`GetComponent<ReziAudioEmitter>()`,
`GetComponents<ReziAudioEmitter@>()`, `AddComponent`, component casts, clip,
volume, pitch, spatial mode, playback state and Play/Pause/Resume/Stop. The
listener is also a typed component handle. The longer event/playback contract
above is the next scripting layer, after event assets exist.

## Node editor plan

The future window will use the repository's existing
`thedmd/imgui-node-editor` target. UI objects edit only `SoundGraphAsset`.
They never own runtime DSP nodes.

The editor model will provide:

- one node catalog generated from node descriptors;
- typed pin colors and link compatibility checks;
- searchable add-node popup and drag-from-pin creation;
- graph parameters and local variables;
- compile diagnostics attached to nodes and pins;
- preview transport using an isolated graph instance;
- live meters copied from the audio thread;
- undo/redo commands storing graph data diffs;
- versioned graph migration before deserialization;
- deterministic JSON output for source control.

The standalone `ReziAudioGraphLab` now exercises this contract with the real
`thedmd/imgui-node-editor` integration. The engine editor asset window remains
a separate integration step.

### Implemented graph slice

`NodeRegistry` is the single catalog for editor presentation and runtime pin
contracts. It currently contains 76 input, parameter, array, math, music,
logic, vector and audio-control node types. `SoundGraphCompiler` validates IDs, node types, pin
directions, compatible value types, single-input ownership, cycles and the
single Audio Output requirement. Its output is a compiled prototype with a
stable evaluation order and resolved input-source table.

`SoundGraphInstance` owns mutable parameters and random state separately from
the prototype. Two instances of the same asset therefore never share runtime
values. Parameters can be addressed by stable FNV-1a ID or by name and can be
updated without recompiling. The first slice evaluates playback configuration
at trigger time and applies settings and transforms live to miniaudio voices.

The second slice adds `DspGraphStream`, a real sample-buffer graph exposed to
miniaudio as a custom `ma_data_source`. Graph compilation decodes Wave Player
clips, validates and sorts audio routes, sizes every processing buffer, delay
line and reverb line, and produces a playback instance before the voice starts.
The callback then performs no allocation, file I/O, logging, reflection or
locking.

Implemented DSP processors:

- predecoded Wave Player with sample-rate/channel conversion, start offset and
  finite or infinite loop counts;
- Sine Oscillator and deterministic white, pink and Brownian Noise generators;
- sample-accurate Repeat Trigger, Delayed Trigger and Trigger Counter controls;
- retriggerable AD Envelope with attack/decay curves;
- Gain and equal-power stereo Pan;
- one-pole Low Pass and High Pass filters;
- feedback Delay with compile-time maximum storage;
- stereo Schroeder-style Reverb with pre-delay, width, wet and dry controls,
  four comb and two all-pass stages;
- Add, Subtract, Multiply, Minimum, Maximum, Clamp and Map Range audio math;
- up to eight-input Mixer;
- explicit DSP Output and reusable sample-buffer fan-out.

Live DSP parameters use a bounded 256-entry SPSC queue. The gameplay/editor
thread resolves node and parameter IDs before enqueueing. The callback only
consumes fixed-size commands and updates preallocated processor state.

Named events use the same bounded realtime queue. `Event Input` nodes register
their event name during graph compilation. Calling `PostEvent("Impact")` on a
`ReziAudioEmitter` enqueues the event without allocation; every matching node
emits a one-sample trigger pulse that can drive envelopes, counters and other
trigger inputs. Event names belong to the graph, so gameplay does not need a
separate event asset. A future event asset can remain a lightweight graph plus
event-name reference without changing the runtime contract.

## Current implementation

- backend abstraction and miniaudio implementation;
- shared engine/device with the legacy system;
- generation-safe voice handles;
- independent voices per playback;
- Master/Music/SFX/Ambience/UI buses;
- live volume, pitch, pan and loop settings; stream/decode mode is selected
  when a voice is created;
- fade-in, fade-to, scheduled fade-out stop, seek, cursor, duration and
  playback-percentage voice controls;
- live position, direction, velocity, listener and full miniaudio 3D
  attenuation/cone/doppler settings;
- serializable `ReziAudioEmitter` and `ReziAudioListener` components;
- editor component controls for clip selection, transport and 3D tuning;
- typed AngelScript component access and emitter transport/properties;
- AngelScript float, int, bool, Vector2, Vector3, float-array, integer-array,
  native `AudioClip@` and `array<AudioClip@>` graph parameter access;
- named graph events through `HasEvent()` and `PostEvent()`;
- UUID-backed `AudioClipReference` values whose runtime source is resolved
  through the engine resource manager;
- shared parameter descriptors and widgets used by both graph labs;
- graph registry, validator/compiler and isolated runtime instances;
- allocation-free callback DSP graph and miniaudio custom stream voices;
- Wave/generator/envelope/trigger/audio-math/Gain/Pan/filter/delay/reverb/
  mixer/output DSP processors;
- bounded lock-free live DSP parameter queue and callback statistics;
- headless backend/graph tests, the ImGui 2D/3D Audio Lab and the node-based
  ReziAudio Graph and DSP Graph Labs.

## Test strategy

`ReziAudioFoundationTests` creates a real WAV fixture, initializes miniaudio
without a device, and verifies initialization, creation, playback, live
settings, live 3D transforms, listener updates, buses, pause/resume, fades,
seek, duration/cursor/percentage queries, cleanup, slot reuse and
stale-generation rejection.

`ReziAudioLab` is a manual UI/integration executable. It uses the same backend
as the new components, plays the engine's `ding.wav`, exposes live voice and 3D
controls, and can orbit a moving emitter around the listener. Its `--smoke`
mode creates a hidden OpenGL/ImGui window, runs the backend without a device,
plays the fixture and exits after six rendered frames.

`ReziAudioGraphTests` verifies catalog coverage, compilation, cycle
diagnostics, math/logic evaluation, stable parameter IDs, instance isolation
and live parameter changes, including typed audio-asset parameters.
`ReziAudioGraphLab --smoke` compiles and plays the default graph through a
no-device miniaudio engine, renders the real node editor for several frames
and changes a parameter while the voice exists. Its runtime list is sourced
from the last successfully compiled parameter set.

`ReziAudioDspGraphTests` additionally counts global C++ allocations around
`ReadFrames`, verifies a zero-allocation callback, exercises every implemented
processor including all three trigger nodes and all three noise colors, checks
finite output, propagated graph duration, cycle rejection, bounded queue
overflow and drives the same graph through `ma_engine_read_pcm_frames`.

`ReziAudioDspGraphLab` is the manual real-time DSP editor. Nodes and audio
routes can be added, connected and deleted. Compiling rebuilds all callback
storage outside the audio thread and refreshes the data-driven runtime
parameter snapshot; Gain, Pan, Cutoff, Delay and Reverb values then update
live without recompilation.

Run:

```powershell
cmake --build --preset debug-vs2026 --config Debug --target ReziAudioLab
.\build\vs2026\bin\Debug\ReziAudioLab.exe
cmake --build --preset debug-vs2026 --config Debug --target ReziAudioGraphLab
.\build\vs2026\bin\Debug\ReziAudioGraphLab.exe
cmake --build --preset debug-vs2026 --config Debug --target ReziAudioDspGraphLab
.\build\vs2026\bin\Debug\ReziAudioDspGraphLab.exe
```

An alternative WAV or OGG can be passed as the first argument. The lab imports
it into its temporary asset catalog; graph and DSP nodes still store an
`AudioClipReference`, not a path value.

## Next implementation phases

1. Register graph, mixer and attenuation as real EGE resources with UUID-based
   references and import metadata; add optional AudioEvent aliases only when
   concurrency/cooldown authoring needs reusable gameplay-facing definitions.
2. Add a separate lock-free event/completion queue from the audio callback.
3. Add click-free atomic graph-instance replacement and parameter smoothing.
4. Add AngelScript playback/event wrappers and graph/event resource handles.
5. Add concurrency groups, priorities, voice stealing and virtualization.
6. Add effects, sends, snapshots, meters and device-change recovery.
7. Integrate the proven Graph Lab UI into the editor asset workflow with
   serialization, undo/redo and graph migrations.
8. Add an explicit legacy-component migration tool, then mark legacy creation
   UI as deprecated.
