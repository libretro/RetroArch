# Vulkan synchronous HDR readback fence

Run `python3 samples/gfx/vulkan_readback_slot/run.py` from the root.
The runner compiles the production HDR staging-slot selection and fence wait
against a fake context. Cached-frame presentation advances the frame index;
the wait must still use the fence belonging to the captured staging slot.
All three slots, idle capture, and the queue-drain fallback are exercised.
The previous source fails the fence identity assertion at runtime.

This checks CPU synchronization selection, not GPU output, display HDR
capability, tone mapping, or presentation brightness. Native GPU conformance
requires separate full-frontend captures.
