# FireVoxel — Parametric Maps for Dynamic Imaging

Reference C++ implementations of several early parametric‑map models used by [FireVoxel](https://firevoxel.org) to analyze dynamic (4D) medical images such as DCE‑MRI, CT, PET, and SPECT. These models underpin FireVoxel’s **Dynamic Analysis → Calculate Parametric Map** workflow and are shared here for transparency, education, and community contributions.

> **Status:** Initial public set with the following models implemented:
> `Model0.cpp`, `Model1.cpp`, `Model3.cpp`, `Model4.cpp`, `Model5.cpp`, `Model6.cpp`.

---

## Table of contents

- [What is this repository?](#what-is-this-repository)
- [Included models](#included-models)
- [Repository layout](#repository-layout)
- [Use with FireVoxel (no coding required)](#use-with-firevoxel-no-coding-required)
- [Build locally (optional, for contributors)](#build-locally-optional-for-contributors)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [Citing this work](#citing-this-work)
- [Research‑use notice](#research-use-notice)
- [Acknowledgments](#acknowledgments)

---

## What is this repository?

This repo hosts compact C++ sources that illustrate how FireVoxel implements parametric analysis for dynamic imaging. The files mirror the numbering used inside FireVoxel’s **Calculate Parametric Map** dialog (e.g., Model 0, Model 1, …), making it easy to cross‑reference algorithms with the user documentation and the application UI.

If you use FireVoxel’s UI, you don’t need to build this code—see the section below for point‑and‑click usage.

---

## Included models

Short, doc-linked labels below follow the numbering in FireVoxel’s **Models** page. See the full list at:  
https://firevoxel.org/docs/html/userguide/models.html

- **Model 0 — Signal measurements** (`Model0.cpp`)  
  https://firevoxel.org/docs/html/userguide/models.html#id4
- **Model 1 — Signal intensity** (`Model1.cpp`)  
  https://firevoxel.org/docs/html/userguide/models.html#id8
- **Model 3 — Interleaved 2‑state profile** (`Model3.cpp`)  
  https://firevoxel.org/docs/html/userguide/models.html#id16
- **Model 4 — Reference curve distance and correlation (1IF)** (`Model4.cpp`)  
  https://firevoxel.org/docs/html/userguide/models.html#id20
- **Model 5 — Time of active rise** (`Model5.cpp`)  
  https://firevoxel.org/docs/html/userguide/models.html#id24
- **Model 6 — (reserved in docs)** (`Model6.cpp`)  
  Placeholder name in documentation; see the Models page above for updates.

> **Note:** Only models compatible with the current dataset are shown in FireVoxel; compatibility is determined automatically from DICOM metadata.

---

## Repository layout

