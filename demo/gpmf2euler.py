#!/usr/bin/env python3
"""Turn a gpmf-parser text dump of CORI/IORI/GRAV into a per-frame yaw/pitch/roll CSV.

    gpmf-parser clip.360 -a -c -fCORI,IORI,GRAV -p9 | gpmf2euler.py --state allon -o euler.csv

Input is a file argument or, by default, stdin. Angles are SciPy intrinsic 'YXZ' in degrees: yaw about Y (up),
pitch about X, roll about Z. For v360 (default) these go to sendcmd directly, used with reset_rot=1. For
--filter v360_vulkan the negated angles are written, used with rorder=rpy. An example ffmpeg command is printed.

    gpmf-parser clip.360 -a -c -fCORI,IORI,GRAV -p9 | gpmf2euler.py --sendcmd cmds.txt [--filter v360_vulkan]
"""

import argparse
import math
import re
import sys

import numpy as np
from scipy.spatial.transform import Rotation

F = np.float32
IDENTITY = np.array([0, 0, 0, 1], dtype=F)  # x, y, z, w
STATES = ("alloff", "antishake", "worldlock", "horizonlevel", "allon")

# float32 overflow/NaN behaviour
np.seterr(all="ignore")


def vec(x, y, z):
    return np.array([x, y, z], dtype=F)


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return np.array([
        ax * bw + ay * bz - az * by + aw * bx,
        -ax * bz + ay * bw + az * bx + aw * by,
        ax * by - ay * bx + az * bw + aw * bz,
        -ax * bx - ay * by - az * bz + aw * bw,
    ], dtype=F)


def conj(q):
    return np.array([-q[0], -q[1], -q[2], q[3]], dtype=F)


def norm_sq(q):
    return q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]


def magnitude(q):
    return np.sqrt(norm_sq(q))


def inverted(q):
    return conj(q) / norm_sq(q)


def normalized(q):
    ls = norm_sq(q)
    if ls == 0:
        return IDENTITY
    return q * (F(1) / np.sqrt(ls))


def vnormalized(v):
    ls = v[0] * v[0] + v[1] * v[1] + v[2] * v[2]
    if ls == 0:
        return vec(0, 0, 0)
    return v * (F(1) / np.sqrt(ls))


def vlength(v):
    return np.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])


def cross(a, b):
    return vec(a[1] * b[2] - a[2] * b[1],
               a[2] * b[0] - a[0] * b[2],
               a[0] * b[1] - a[1] * b[0])


def rotated_vector(q, v):
    return qmul(qmul(q, np.array([v[0], v[1], v[2], 0], dtype=F)), conj(q))[:3]


def from_axis_angle(axis, radians):
    t = radians * F(0.5)
    s = np.sin(t)
    return np.array([axis[0] * s, axis[1] * s, axis[2] * s, np.cos(t)], dtype=F)


def from_axes(xa, ya, za):
    m00, m01, m02 = xa[0], ya[0], za[0]
    m10, m11, m12 = xa[1], ya[1], za[1]
    m20, m21, m22 = xa[2], ya[2], za[2]
    trace = m00 + m11 + m22
    if trace > 0:
        s = F(0.5) / np.sqrt(trace + F(1))
        w = F(0.25) / s
        x, y, z = (m21 - m12) * s, (m02 - m20) * s, (m10 - m01) * s
    elif m00 > m11 and m00 > m22:
        s = F(2) * np.sqrt(F(1) + m00 - m11 - m22)
        w, x, y, z = (m21 - m12) / s, F(0.25) * s, (m01 + m10) / s, (m02 + m20) / s
    elif m11 > m22:
        s = F(2) * np.sqrt(F(1) + m11 - m00 - m22)
        w, x, y, z = (m02 - m20) / s, (m01 + m10) / s, F(0.25) * s, (m12 + m21) / s
    else:
        s = F(2) * np.sqrt(F(1) + m22 - m00 - m11)
        w, x, y, z = (m10 - m01) / s, (m02 + m20) / s, (m12 + m21) / s, F(0.25) * s
    return np.array([x, y, z, w], dtype=F)


def rotation_angle(q):
    u = q * (F(1) / magnitude(q))
    if np.sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]) == 0:
        return F(0)
    return F(2) * np.arccos(u[3])


def cori_filter(cori, frame_rate):
    """Freezes accumulated rotation after 5 steady frames; only used for timewarp (frame_rate >= 0)."""
    out = [IDENTITY] * len(cori)
    if frame_rate < 0:
        return out

    prev, cur, steady = IDENTITY, IDENTITY, 0
    for i, c in enumerate(cori):
        r = qmul(c, inverted(prev))
        prev = c
        speed = F(float(rotation_angle(r)) * 180.0 / math.pi * float(frame_rate))
        steady = steady + 1 if speed < 0.5 else 0
        if steady < 5:
            cur = qmul(r, cur)
        out[i] = cur
    return out


def process_roll(cam, prev_cam, prev_roll, prev_angle, index):
    roll_max = F(20 * math.pi / 180)
    roll_lock = F(20 * math.pi / 180)
    roll_strength = F(0.8)

    if index > 0:
        d = qmul(qmul(qmul(conj(prev_roll), conj(cam)), prev_cam), prev_roll)
        x = F(2) * d[0] * d[1] - F(2) * d[2] * d[3]
        y = F(1) - F(2) * d[0] * d[0] - F(2) * d[2] * d[2]
        cam_angle = np.arctan2(x, y)
        angle = prev_angle - cam_angle
    else:
        cam_angle = prev_angle
        angle = prev_angle

    angle = angle * roll_strength
    angle = max(-roll_max, min(roll_max, angle))
    if np.abs(cam_angle) > roll_lock:
        angle = prev_angle

    half = angle / F(2)
    return np.array([0, 0, np.sin(half), np.cos(half)], dtype=F), angle


class Clip:
    def __init__(self, cori, iori, grav, frame_rate):
        self.cori, self.iori, self.grav = cori, iori, grav
        self.frame_rate = F(frame_rate)
        self.cori_filtered = cori_filter(cori, self.frame_rate)
        self.n = len(cori)

    def cori_at(self, i):
        return self.cori_filtered[i] if self.frame_rate > -1 else self.cori[i]

    def orient(self, i):
        return conj(normalized(qmul(self.iori[i], self.cori_at(i))))

    def antishake_states(self):
        out = []
        prev_cam, prev_roll, prev_angle = IDENTITY, IDENTITY, F(0)
        for i in range(self.n):
            cam = self.orient(i)
            roll, prev_angle = process_roll(cam, prev_cam, prev_roll, prev_angle, i)
            prev_cam, prev_roll = cam, roll
            out.append(qmul(cam, roll))
        return out

    def horizon_quat(self, i):
        g = vnormalized(self.grav[i])
        x = vnormalized(cross(vec(0, 0, 1), g))
        z = vnormalized(cross(g, x))
        return qmul(inverted(self.cori_at(i)), from_axes(-x, g, z))

    def worldlock_states(self):
        out = []
        follow = IDENTITY
        for i in range(self.n):
            s = self.horizon_quat(i)
            if magnitude(follow + s) < magnitude(follow - s):
                s = -s
            follow = s
            out.append(s)
        return out

    def window(self, states, i, half):
        lo = max(0, i - half)
        hi = min(self.n - 2, i + half)
        q = np.zeros(4, dtype=F)
        for k in range(lo, hi + 1):
            q = q + states[k] * F(half + 1 - abs(k - i))
        return normalized(q)

    def all_on(self, cori, iori, grav, north):
        cam = normalized(qmul(iori, cori))
        g = vnormalized(rotated_vector(inverted(cori), grav))

        if north:
            axe = cross(vec(0, 1, 0), g)
            angle = self.asin_clamped(vlength(axe))
            if g[1] < 0:
                angle = F(math.pi - float(angle))
            grav_rot = from_axis_angle(vnormalized(axe), angle)
        else:
            axe = cross(vec(0, -1, 0), g)
            angle = self.asin_clamped(vlength(axe))
            if g[1] > 0:
                angle = F(math.pi - float(angle))
            grav_rot = qmul(from_axis_angle(vnormalized(axe), angle), IDENTITY_Z180)

        return qmul(cam, normalized(grav_rot))

    @staticmethod
    def asin_clamped(x):
        return np.arcsin(F(1)) if x > 1 else np.arcsin(x)

    def meld(self, state):
        # The pole is picked once from frame 0 (raw cori) and kept so the result has no discontinuities.
        g0 = vnormalized(rotated_vector(inverted(self.cori[0]), self.grav[0]))
        north = g0[1] > 0

        stab = None
        if state == "antishake":
            stab = self.antishake_states()
        elif state == "horizonlevel":
            stab = self.worldlock_states()

        out = []
        for i in range(self.n):
            cori, iori = self.cori_at(i), self.iori[i]
            if state == "alloff":
                out.append(IDENTITY)
            elif state == "antishake":
                out.append(qmul(qmul(iori, cori), self.window(stab, i, 30)))
            elif state == "worldlock":
                out.append(normalized(qmul(iori, cori)))
            elif state == "horizonlevel":
                out.append(qmul(qmul(iori, cori), self.window(stab, i, 20)))
            else:
                out.append(self.all_on(cori, iori, self.grav[i], north))
        return np.array(out, dtype=F)


IDENTITY_Z180 = np.array([0, 0, 1, 0], dtype=F)

SAMPLE_RE = re.compile(r"^\s*(CORI|IORI|GRAV)\s+(.*)$")
FRAMERATE_RE = re.compile(r"^\s*([0-9.]+) with (\d+) frames")


def parse_dump(lines):
    samples = {"CORI": [], "IORI": [], "GRAV": []}
    fps = frames = None

    for line in lines:
        m = SAMPLE_RE.match(line)
        if m:
            values = [float(v) for v in m.group(2).split(",") if v.strip()]
            samples[m.group(1)].append(values)
        elif fps is None:
            m = FRAMERATE_RE.match(line)
            if m:
                fps, frames = float(m.group(1)), int(m.group(2))

    return samples, fps, frames


def exact_fps(fps):
    # snap to the NTSC rationals so per-frame times don't drift
    for r in (24000, 30000, 60000, 120000, 240000):
        if abs(fps - r / 1001) < 0.001:
            return r / 1001
    return fps


def write_sendcmd(path, euler, fps, filt):
    if fps is None:
        raise SystemExit("--sendcmd needs the VIDEO FRAMERATE line in the dump")

    # v360_vulkan has no reset_rot and applies the inverse rotation: negated angles with rorder=rpy
    sign = -1 if filt == "v360_vulkan" else 1
    fps = exact_fps(fps)
    with open(path, "w") as out:
        for i, (yaw, pitch, roll) in enumerate(euler):
            # half a frame early so rounding can't push a command onto the next frame
            t = max(0.0, (i - 0.5) / fps)
            out.write(f"{t:.6f} {filt} yaw {sign * yaw:.6f}, {filt} pitch {sign * pitch:.6f}, "
                      f"{filt} roll {sign * roll:.6f};\n")


def example_command(filt, cmds):
    if filt == "v360_vulkan":
        return ("ffmpeg -hide_banner -init_hw_device vulkan=vk -filter_hw_device vk -hwaccel vulkan \\\n"
                "  -hwaccel_device vk -hwaccel_output_format vulkan -i INPUT.360 -filter_complex \\\n"
                f"  \"[0:0]sendcmd=f={cmds}[vf];[vf][0:4]v360_vulkan=input=gopromax:output=e:w=7680:h=3840:rorder=rpy[vout]\" \\\n"
                "  -map \"[vout]\" -c:v hevc_vulkan -map 0:a:0 -c:a copy -y out.mp4")
    return f"ffmpeg -i eac.mp4 -vf sendcmd=f={cmds},v360=input=eac:output=e:reset_rot=1 out.mp4"


def build_clip(samples, fps, frames, rate, fsfb):
    counts = {k: len(v) for k, v in samples.items()}
    if counts["GRAV"] == 0:
        raise SystemExit("no GRAV samples in dump")

    n = min(counts.values())
    if max(counts.values()) - n > 2:
        raise SystemExit(f"CORI/IORI/GRAV sample counts differ by more than 2: {counts}")

    if fps is not None:
        max_missing = int(fps) + (1 if fps > int(fps) else 0)
        needed = 0 if frames < max_missing else frames - max_missing
        if min(counts.values()) < needed:
            raise SystemExit(f"too few samples {counts} for {frames} frames")

    # GPMF stores w,x,y,z; this needs x,y,z,w
    def quats(key):
        return np.array([[v[1], v[2], v[3], v[0]] for v in samples[key][:n]], dtype=F)

    cori, iori = quats("CORI"), quats("IORI")
    grav = np.array([v[:3] for v in samples["GRAV"][:n]], dtype=F)

    if not cori.any():
        raise SystemExit("all CORI samples are zero")
    if fsfb:
        iori[:] = IDENTITY
    elif not iori.any():
        raise SystemExit("all IORI samples are zero")

    if rate is None:
        frame_rate = -1
    elif fps is None:
        raise SystemExit("--rate needs the VIDEO FRAMERATE line in the dump")
    else:
        frame_rate = F(math.floor(fps + 1e-6)) / F(rate)

    return Clip(list(cori), list(iori), list(grav), frame_rate), not grav.any()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dump", nargs="?", default="-", help="gpmf-parser output (default: stdin)")
    ap.add_argument("-o", "--output", help="CSV file (default stdout, unless --sendcmd is given)")
    ap.add_argument("--sendcmd", metavar="FILE", help="write an ffmpeg sendcmd file with one line per frame")
    ap.add_argument("--filter", choices=("v360", "v360_vulkan"), default="v360",
                    help="ffmpeg filter the sendcmd file targets (default: v360)")
    ap.add_argument("--state", choices=STATES, default="allon")
    ap.add_argument("--rate", type=float, help="timewarp speed, e.g. 5 for RATE=5X (not in the dump)")
    ap.add_argument("--fsfb", action="store_true", help="PRJT is FSFB: ignore IORI (not in the dump)")
    args = ap.parse_args()

    if args.dump == "-" and sys.stdin.isatty():
        ap.error("no input: give a dump file or pipe gpmf-parser output")

    with (sys.stdin if args.dump == "-" else open(args.dump)) as f:
        samples, fps, frames = parse_dump(f)

    clip, grav_invalid = build_clip(samples, fps, frames, args.rate, args.fsfb)
    state = args.state
    if grav_invalid:
        print("GRAV is all zero, using antishake", file=sys.stderr)
        state = "antishake"

    quats = clip.meld(state)
    euler = Rotation.from_quat(quats.astype(np.float64)).as_euler("YXZ", degrees=True)

    if args.sendcmd:
        write_sendcmd(args.sendcmd, euler, fps, args.filter)
        print(f"\nExample:\n{example_command(args.filter, args.sendcmd)}", file=sys.stderr)

    if args.output or not args.sendcmd:
        with (open(args.output, "w") if args.output else sys.stdout) as out:
            out.write("frame,yaw,pitch,roll\n")
            for i, (yaw, pitch, roll) in enumerate(euler):
                out.write(f"{i},{yaw:.6f},{pitch:.6f},{roll:.6f}\n")


if __name__ == "__main__":
    main()
