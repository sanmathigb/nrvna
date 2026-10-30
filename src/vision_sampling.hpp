#pragma once

namespace nrvna::detail {

inline float resolveVisionTemperature(float configured_temp,
                                      bool has_model_hint,
                                      bool has_general_override,
                                      bool has_vision_override,
                                      float vision_override,
                                      float vision_fallback = 0.3f) {
    if (has_vision_override) return vision_override;
    if (has_general_override || has_model_hint) return configured_temp;
    return vision_fallback;
}

} // namespace nrvna::detail
