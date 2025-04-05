#pragma once
#ifndef BARRIERBEHAVIOR_HPP
#define BARRIERBEHAVIOR_HPP

enum class BarrierBehavior
{
  /// Inherits it's value from etna configuration
  eDefault,

  /// Explicit way to set behavior. Ignores etna config
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

#endif // BARRIERBEHAVIOR_HPP
