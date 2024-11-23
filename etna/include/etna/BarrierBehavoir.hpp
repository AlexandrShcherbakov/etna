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

#endif // BARRIERBEHAVOIR_HPP
