#!/usr/bin/env python3
"""Reusable procedural profiles for Crownless humanoids and wearable shells.

This module intentionally has no Blender dependency.  It owns the parametric
shape grammar; Blender scripts consume the resulting cross-sections to create
meshes, assign materials, and attach them to a rig.
"""

from __future__ import annotations

from dataclasses import asdict, dataclass
import math
from typing import Iterable


@dataclass(frozen=True)
class CrossSection:
    """An elliptical section along a normalized limb or absolute torso axis."""

    position: float
    width: float
    depth: float
    center_depth: float = 0.0

    def __post_init__(self) -> None:
        if not math.isfinite(self.position):
            raise ValueError("cross-section position must be finite")
        if self.width <= 0.0 or self.depth <= 0.0:
            raise ValueError("cross-section radii must be positive")


@dataclass(frozen=True)
class BodyParameters:
    """Continuous controls for one body on the canonical humanoid skeleton.

    Version 1 varies surface proportions without moving the runtime bones.  A
    future recipe version can derive a matching skeleton from the same values.
    """

    name: str
    muscularity: float = 0.65
    body_fat: float = 0.18
    shoulder_scale: float = 1.0
    chest_scale: float = 1.0
    waist_scale: float = 1.0
    hip_scale: float = 1.0
    arm_scale: float = 1.0
    leg_scale: float = 1.0
    neck_scale: float = 1.0
    hand_scale: float = 1.0
    foot_scale: float = 1.0

    def __post_init__(self) -> None:
        if not self.name:
            raise ValueError("body preset requires a name")
        for field in ("muscularity", "body_fat"):
            value = getattr(self, field)
            if not 0.0 <= value <= 1.0:
                raise ValueError(f"{field} must be between 0 and 1")
        for field in (
            "shoulder_scale", "chest_scale", "waist_scale", "hip_scale",
            "arm_scale", "leg_scale", "neck_scale", "hand_scale", "foot_scale",
        ):
            value = getattr(self, field)
            if not 0.70 <= value <= 1.35:
                raise ValueError(f"{field} must be between 0.70 and 1.35")


@dataclass(frozen=True)
class ShellRecipe:
    """Fit controls used to derive a wearable shell from a body profile."""

    name: str
    kind: str
    clearance_width: float
    clearance_depth: float
    thickness: float
    ease: float = 0.0
    compression: float = 0.0
    width_scale: float = 1.0
    depth_scale: float = 1.0
    center_depth_bias: float = 0.0

    def __post_init__(self) -> None:
        if not self.name or not self.kind:
            raise ValueError("shell recipe requires a name and kind")
        if self.clearance_width < 0.0 or self.clearance_depth < 0.0:
            raise ValueError("shell clearance cannot be negative")
        if self.thickness <= 0.0:
            raise ValueError("shell thickness must be positive")
        if not 0.0 <= self.compression <= 0.30:
            raise ValueError("shell compression must be between 0 and 0.30")


@dataclass(frozen=True)
class CharacterRecipe:
    """Serializable recipe tying one body preset to basic wearable layers."""

    schema_version: int
    body: BodyParameters
    garments: tuple[ShellRecipe, ...]
    equipment: tuple[ShellRecipe, ...]

    def to_manifest(self) -> dict[str, object]:
        return {
            "schema_version": self.schema_version,
            "body": asdict(self.body),
            "garments": [asdict(recipe) for recipe in self.garments],
            "equipment": [asdict(recipe) for recipe in self.equipment],
        }


REFERENCE_MUSCULARITY = 0.68
REFERENCE_BODY_FAT = 0.18


ACTION_FIGURE_WAYFARER = BodyParameters(
    name="screen_readable_action_figure",
    muscularity=0.68,
    body_fat=0.13,
    shoulder_scale=1.05,
    chest_scale=0.98,
    waist_scale=0.84,
    hip_scale=0.92,
    arm_scale=1.02,
    leg_scale=1.00,
    neck_scale=0.92,
    hand_scale=1.08,
    foot_scale=1.10,
)

LEAN_SCOUT = BodyParameters(
    name="lean_scout", muscularity=0.48, body_fat=0.10,
    shoulder_scale=0.94, chest_scale=0.94, waist_scale=0.90,
    hip_scale=0.94, arm_scale=0.90, leg_scale=0.94,
)

HEAVY_VANGUARD = BodyParameters(
    name="heavy_vanguard", muscularity=0.84, body_fat=0.30,
    shoulder_scale=1.08, chest_scale=1.08, waist_scale=1.10,
    hip_scale=1.06, arm_scale=1.10, leg_scale=1.08,
)


PADDED_UNDERLAYER = ShellRecipe(
    name="padded_underlayer", kind="padded_cloth",
    clearance_width=0.012, clearance_depth=0.012, thickness=0.012,
    ease=0.008,
)

FITTED_TUNIC = ShellRecipe(
    name="fitted_tunic", kind="woven_cloth",
    clearance_width=0.024, clearance_depth=0.022, thickness=0.010,
    ease=0.010,
)

CUIRASS_SHELL = ShellRecipe(
    name="cuirass_shell", kind="rigid_armor",
    clearance_width=0.020, clearance_depth=0.026, thickness=0.024,
    ease=0.004, compression=0.14, width_scale=1.0, depth_scale=1.06,
    center_depth_bias=-0.008,
)

FITTED_BRACER = ShellRecipe(
    name="fitted_bracer", kind="rigid_armor",
    clearance_width=0.012, clearance_depth=0.012, thickness=0.014,
    ease=0.002, compression=0.04, width_scale=0.98,
)

FITTED_GREAVE = ShellRecipe(
    name="fitted_greave", kind="rigid_armor",
    clearance_width=0.004, clearance_depth=0.005, thickness=0.016,
    ease=0.002, compression=0.08, depth_scale=1.03,
)


WAYFARER_RECIPE = CharacterRecipe(
    schema_version=1,
    body=ACTION_FIGURE_WAYFARER,
    garments=(PADDED_UNDERLAYER, FITTED_TUNIC),
    equipment=(CUIRASS_SHELL, FITTED_BRACER, FITTED_GREAVE),
)


def _deltas(parameters: BodyParameters) -> tuple[float, float]:
    return (
        parameters.muscularity - REFERENCE_MUSCULARITY,
        parameters.body_fat - REFERENCE_BODY_FAT,
    )


def _sections(rows: Iterable[tuple[float, float, float]]) -> tuple[CrossSection, ...]:
    return tuple(CrossSection(position, width, depth) for position, width, depth in rows)


def body_profiles(parameters: BodyParameters) -> dict[str, tuple[CrossSection, ...]]:
    """Generate canonical anatomical profiles for a humanoid body preset."""
    muscle, fat = _deltas(parameters)
    shoulder = parameters.shoulder_scale * (1.0 + muscle * 0.14 + fat * 0.04)
    chest = parameters.chest_scale * (1.0 + muscle * 0.11 + fat * 0.12)
    waist = parameters.waist_scale * (1.0 + muscle * 0.03 + fat * 0.30)
    hips = parameters.hip_scale * (1.0 + muscle * 0.03 + fat * 0.22)
    arm = parameters.arm_scale * (1.0 + muscle * 0.25 + fat * 0.12)
    leg = parameters.leg_scale * (1.0 + muscle * 0.20 + fat * 0.12)

    profiles = {
        "torso": _sections((
            (1.02, 0.215 * waist, 0.142 * waist),
            (1.12, 0.235 * waist, 0.152 * waist),
            (1.28, 0.265 * ((waist + chest) * 0.5), 0.172 * ((waist + chest) * 0.5)),
            (1.44, 0.325 * chest, 0.190 * chest),
            (1.56, 0.360 * shoulder, 0.198 * chest),
            (1.62, 0.300 * shoulder, 0.170 * chest),
            (1.67, 0.125 * parameters.neck_scale, 0.095 * parameters.neck_scale),
        )),
        "pelvis": _sections((
            (0.86, 0.175 * hips, 0.135 * hips),
            (0.94, 0.220 * hips, 0.165 * hips),
            (1.03, 0.245 * hips, 0.180 * hips),
            (1.10, 0.215 * ((hips + waist) * 0.5), 0.165 * ((hips + waist) * 0.5)),
        )),
        "upper_arm": _sections((
            (0.00, 0.085 * arm, 0.080 * arm),
            (0.25, 0.118 * arm, 0.105 * arm),
            (0.62, 0.100 * arm, 0.088 * arm),
            (1.00, 0.072 * arm, 0.066 * arm),
        )),
        "forearm": _sections((
            (0.00, 0.074 * arm, 0.068 * arm),
            (0.34, 0.098 * arm, 0.083 * arm),
            (0.68, 0.076 * arm, 0.064 * arm),
            (1.00, 0.056 * arm, 0.050 * arm),
        )),
        "thigh": _sections((
            (0.00, 0.120 * leg, 0.108 * leg),
            (0.28, 0.148 * leg, 0.125 * leg),
            (0.68, 0.120 * leg, 0.105 * leg),
            (1.00, 0.088 * leg, 0.080 * leg),
        )),
        "shin": _sections((
            (0.00, 0.086 * leg, 0.080 * leg),
            (0.25, 0.105 * leg, 0.092 * leg),
            (0.56, 0.098 * leg, 0.104 * leg),
            (1.00, 0.065 * leg, 0.058 * leg),
        )),
        "neck": _sections((
            (0.00, 0.092 * parameters.neck_scale, 0.082 * parameters.neck_scale),
            (0.35, 0.080 * parameters.neck_scale, 0.074 * parameters.neck_scale),
            (1.00, 0.076 * parameters.neck_scale, 0.070 * parameters.neck_scale),
        )),
    }
    validate_profiles(profiles)
    return profiles


def derive_shell(
    source: Iterable[CrossSection],
    recipe: ShellRecipe,
) -> tuple[CrossSection, ...]:
    """Derive a fitted shell that contains the exported anatomical surface.

    Compression reduces the layer's fit allowance.  It does not shrink the
    shell through the undeformed body: recipes that eventually compress flesh
    must pair that behavior with an explicit body-deformation or body-culling
    pass.
    """
    sections = tuple(source)
    if len(sections) < 2:
        raise ValueError("a shell requires at least two source sections")
    low = sections[0].position
    span = sections[-1].position - low
    if span <= 0.0:
        raise ValueError("source sections must be ordered")
    result = []
    for section in sections:
        normalized = (section.position - low) / span
        bell = math.sin(math.pi * normalized) ** 2
        allowance_scale = 1.0 - recipe.compression
        center_offset = recipe.center_depth_bias * bell
        result.append(CrossSection(
            section.position,
            max(section.width, section.width * recipe.width_scale)
            + (recipe.clearance_width + recipe.ease * bell) * allowance_scale,
            max(section.depth, section.depth * recipe.depth_scale)
            + (recipe.clearance_depth + recipe.ease * bell) * allowance_scale
            + abs(center_offset),
            section.center_depth + center_offset,
        ))
    return tuple(result)


def sample_profile(
    sections: Iterable[CrossSection],
    position: float,
) -> CrossSection:
    """Linearly sample an ordered profile at an in-range position."""
    profile = tuple(sections)
    if not profile:
        raise ValueError("cannot sample an empty profile")
    if position <= profile[0].position:
        return CrossSection(position, profile[0].width, profile[0].depth,
                            profile[0].center_depth)
    if position >= profile[-1].position:
        return CrossSection(position, profile[-1].width, profile[-1].depth,
                            profile[-1].center_depth)
    for left, right in zip(profile, profile[1:]):
        if left.position <= position <= right.position:
            amount = (position - left.position) / (right.position - left.position)
            return CrossSection(
                position,
                left.width + (right.width - left.width) * amount,
                left.depth + (right.depth - left.depth) * amount,
                left.center_depth + (right.center_depth - left.center_depth) * amount,
            )
    raise AssertionError("ordered profile sampling failed")


def clip_profile(
    sections: Iterable[CrossSection],
    start: float,
    end: float,
    *,
    normalize: bool = False,
) -> tuple[CrossSection, ...]:
    """Clip a profile to a coverage interval, optionally renormalizing it."""
    if end <= start:
        raise ValueError("profile clip end must be greater than start")
    profile = tuple(sections)
    positions = [start]
    positions.extend(section.position for section in profile if start < section.position < end)
    positions.append(end)
    clipped = [sample_profile(profile, position) for position in positions]
    if not normalize:
        return tuple(clipped)
    span = end - start
    return tuple(CrossSection(
        (section.position - start) / span,
        section.width, section.depth, section.center_depth,
    ) for section in clipped)


def loft_rows(sections: Iterable[CrossSection]) -> list[tuple[float, float, float, float]]:
    """Convert sections into the torso-loft format used by the Blender builder."""
    return [
        (section.position, section.width, section.depth, section.center_depth)
        for section in sections
    ]


def sweep_rows(sections: Iterable[CrossSection]) -> list[tuple[float, float, float, float]]:
    """Convert sections into normalized limb-sweep rows."""
    return [
        (section.position, section.width, section.depth, section.center_depth)
        for section in sections
    ]


def validate_profiles(profiles: dict[str, tuple[CrossSection, ...]]) -> None:
    required = {"torso", "pelvis", "upper_arm", "forearm", "thigh", "shin", "neck"}
    if missing := required - profiles.keys():
        raise ValueError(f"missing body profiles: {sorted(missing)}")
    for name, sections in profiles.items():
        if len(sections) < 2:
            raise ValueError(f"{name} requires at least two cross-sections")
        if any(right.position <= left.position for left, right in zip(sections, sections[1:])):
            raise ValueError(f"{name} cross-sections must be strictly ordered")


def validate_library() -> None:
    """Fast dependency-free contract check for presets and derived shells."""
    for preset in (ACTION_FIGURE_WAYFARER, LEAN_SCOUT, HEAVY_VANGUARD):
        profiles = body_profiles(preset)
        for recipe in WAYFARER_RECIPE.garments + WAYFARER_RECIPE.equipment:
            region = "forearm" if recipe.name == "fitted_bracer" else (
                "shin" if recipe.name == "fitted_greave" else "torso"
            )
            source = profiles[region]
            shell = derive_shell(source, recipe)
            for body_section, shell_section in zip(source, shell):
                if shell_section.width <= body_section.width:
                    raise ValueError(f"{preset.name}/{recipe.name} width clearance collapsed")
                usable_depth = shell_section.depth - abs(
                    shell_section.center_depth - body_section.center_depth
                )
                if usable_depth <= body_section.depth:
                    raise ValueError(f"{preset.name}/{recipe.name} depth clearance collapsed")


if __name__ == "__main__":
    validate_library()
    print("Validated procedural body, garment, and equipment profile library.")
