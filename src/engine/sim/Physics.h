#pragma once
// The ball simulation, reproducing the original's integer arithmetic step for step.
// See docs/engine-physics.md for where each constant and rule comes from.
#include <array>
#include <functional>

#include "engine/data/Bumpers.h"
#include "engine/data/TableData.h"
#include "engine/data/TableLayout.h"
#include "engine/sim/Ball.h"
#include "engine/sim/Flipper.h"
#include "engine/sim/Mask.h"
#include "engine/data/PhysmapPatches.h"

namespace encore {

/// Collision constants for one material class (16-byte records at ds:0x231d).
struct Material {
  i16 tangentGain = 0;     ///< divides the tangential correction
  i16 spinGain = 0;        ///< divides the spin correction
  i16 restitution = 0;     ///< the bounce keeps 1 - 256/restitution of the normal speed
  i16 minNormalSpeed = 0;  ///< negative; slower approaches do not bounce at all
  i16 glanceLimit = 0;     ///< tangent-to-normal ratio above which the hit is a graze
};

/// What sampling the masks around the ball found.
struct Contact {
  bool hit = false;
  int count = 0;          ///< how many probes are inside a solid pixel
  int angle = 0;          ///< surface normal, 0..0x7ff, pointing into the surface
  int materialClass = 0;
  int pointX = 0, pointY = 0;
};

class Physics {
 public:
  /// Global constants from ds:0x68a2.
  struct Limits {
    i16 velocityMin = -4100, velocityMax = 4100;
    i16 bumperKick = -7000, slingshotKick = -2000, slingshotMinimum = -300;
    i16 nudgeLift = 600, nudgeReturn = -200;
  };

  struct Controls {
    bool leftFlipper = false;
    bool rightFlipper = false;
    bool nudge = false;
  };

  void init(const TableData& table, bool highResolution);

  /// Once per video frame, before the sub-steps: picks the gravity zone and decays the tilt.
  void beginFrame(Ball& ball);
  /// One physics sub-step. There are three per frame in high resolution, four otherwise.
  void subStep(Ball& ball, const Controls& controls);

  int subStepsPerFrame() const { return highResolution_ ? 3 : 4; }

  Contact probe(const Ball& ball) const;
  const Mask& walls() const { return walls_; }
  Mask& walls() { return walls_; }
  const std::array<Flipper, 3>& flippers() const { return flippers_; }
  std::array<Flipper, 3>& flippers() { return flippers_; }

  /// Tilt state.
  bool tilted() const { return tilted_; }
  void addTilt(int amount);
  void decayTilt();
  int tiltCounter() const { return tiltCounter_; }
  void setFlippersEnabled(bool on) { flippersEnabled_ = on; }

  /// Stamps a gate or drop target into its layer's solid plane.
  void setPatch(const PhysmapPatch& patch, bool raised);

  /// The bumper or kicker struck by the most recent bounce, or -1.
  int takeBumper();

  /// Where the ball last touched something solid, and whether it did so this frame.
  bool touched() const { return touched_; }
  Point contactPoint() const { return contact_; }
  void clearContact() { touched_ = false; }

  /// Applies the plunger launch: `pull` is the held counter, 0..32.
  void launch(Ball& ball, int pull, u32 randomValue) const;

  /// Installs the table's bumpers and kickers. The list must outlive the simulation.
  void setBumpers(const std::vector<Bumper>* bumpers) { bumpers_ = bumpers; }

  const Limits& limits() const { return limits_; }
  const TableLayout& layout() const { return layout_; }

 private:
  void integrate(Ball& ball) const;
  void bounce(Ball& ball, const Contact& contact);
  void updateNudgeAndFlippers(Ball& ball, const Controls& controls);
  void stampFlippers(const Ball& ball);
  void selectGravity(Ball& ball);
  void updateLayer(Ball& ball) const;
  int flipperImpulse(const Ball& ball, int px, int py, int& fx, int& fy) const;
  int clampVelocity(int v) const;

  const TableData* table_ = nullptr;
  TableLayout layout_;
  bool highResolution_ = true;

  Mask walls_;        ///< lower-layer walls, mutated by flipper and drop-target stamps
  Mask rampWalls_;    ///< upper-layer walls, also mutated
  Mask dynamic_;      ///< bumper and target marks
  const Mask* zoneLower_ = nullptr;   ///< occlusion mask doubling as the gravity-zone map
  const Mask* zoneUpper_ = nullptr;
  const Mask* rubberUpper_ = nullptr;

  std::array<i16, 2560> sine_{};
  std::array<Material, 8> materials_{};
  /// One acceleration per ramp zone. Index 0 is level ground; the rest are the table's
  /// rails and ramps, and there are far more of them on some tables than others.
  std::vector<Point> ramps_;
  Point currentGravity_{};
  Limits limits_;
  std::array<Flipper, 3> flippers_{};
  std::array<Mask, 3> flipperMasks_{};  ///< frame stacks, shaped by each record's geometry
  const std::vector<Bumper>* bumpers_ = nullptr;
  std::vector<Rect> zonesWhenUpper_, zonesWhenLower_;

  // Per-hit values the response needs, matching ds:0x6894 and ds:0x6896.
  int flipperVx_ = 0, flipperVy_ = 0;
  int pendingBumper_ = -1;
  bool touched_ = false;
  Point contact_{};
  int firedBumper_ = -1;

  // Nudge state (ds:0x2316, 0x2318, 0x231a).
  int nudgeAccumulator_ = 0;
  int nudgeLiftPixels_ = 0;
  int tableVelocity_ = 0;
  int tiltCounter_ = 0;
  bool tilted_ = false;
  bool flippersEnabled_ = true;

  i16 sin(int angle) const { return sine_[static_cast<std::size_t>(angle) & 0x7ff]; }
  i16 cos(int angle) const { return sine_[(static_cast<std::size_t>(angle) & 0x7ff) + 512]; }
};

}  // namespace encore
