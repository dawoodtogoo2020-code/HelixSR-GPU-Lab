# Provenance and distribution boundaries

## Included

The support verifier, public documentation/site and CI were authored for this project. The metadata contract, synthetic tests and benchmark comparator were developed in the project's host-only portability work, then made self-contained for this release. Their metadata checks and reference fixtures are not model implementations or game code. Original included files are covered by the repository's MIT license.

## Excluded

No commercial game executable, data archive, extracted shader, Sony SDK/library, model weight, translated neural shader body, private lab runtime binary, console key, credential, raw game dump or private installation receipt is included. The private research repository has not been made public.

The lab's native UI, generated font/shader assets and compiled dependencies require a separate provenance review before distribution. Their presence on a development machine is not sufficient clearance.

## Upstream references

- [Lone Wolf's HelixSR](https://github.com/lonewolf0622/HelixSR): upstream PC implementation. Its [current license](https://github.com/lonewolf0622/HelixSR/blob/main/LICENSE) limits redistribution of that software to complete, unmodified official releases and separately restricts generated NVIDIA material. Do not treat readable setup code as permission to relicense or port it. No upstream implementation or generated model is included here.
- [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk): public homebrew tooling reference, not bundled or required for the host preview.
- [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan): historical native toolchain reference, not bundled or a claim of release readiness.

Each external project retains its own license. Naming a project does not imply affiliation or endorsement. The MIT license here applies only to material we include and have authored; it does not override upstream or third-party terms.
