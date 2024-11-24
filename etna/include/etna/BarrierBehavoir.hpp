#pragma once
#ifndef BARRIERBEHAVOIR_HPP
#define BARRIERBEHAVOIR_HPP

enum class BarrierBehavoir
{
  /// Inherits it's value from etna configuration
  eDefault,

  /// Explicit way to set behavoir. Ignores etna config
  eSuppressBarriers,
  eGenerateBarriers
};

/// Force set_state to create barriers, even when they seem unnecessary
enum class ForceSetState
{
  // foo(true) is less readable than foo(ForceSetState::eTrue)
  eFalse,
  eTrue
};

#endif // BARRIERBEHAVOIR_HPP
