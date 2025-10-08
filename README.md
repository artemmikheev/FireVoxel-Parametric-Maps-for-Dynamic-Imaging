# FireVoxel — Parametric Maps for Dynamic Imaging

[![Stars](https://img.shields.io/github/stars/artemmikheev/FireVoxel-Parametric-Maps-for-Dynamic-Imaging?style=flat)](https://github.com/artemmikheev/FireVoxel-Parametric-Maps-for-Dynamic-Imaging/stargazers)
[![Issues](https://img.shields.io/github/issues/artemmikheev/FireVoxel-Parametric-Maps-for-Dynamic-Imaging?style=flat)](https://github.com/artemmikheev/FireVoxel-Parametric-Maps-for-Dynamic-Imaging/issues)
[![Pull Requests](https://img.shields.io/github/issues-pr/artemmikheev/FireVoxel-Parametric-Maps-for-Dynamic-Imaging?style=flat)](https://github.com/artemmikheev/FireVoxel-Parametric-Maps-for-Dynamic-Imaging/pulls)
[![Last commit](https://img.shields.io/github/last-commit/artemmikheev/FireVoxel-Parametric-Maps-for-Dynamic-Imaging?style=flat)](https://github.com/artemmikheev/FireVoxel-Parametric-Maps-for-Dynamic-Imaging/commits/main)
[![Docs](https://img.shields.io/badge/docs-Parametric%20Map-0b7285)](https://firevoxel.org/docs/html/userguide/param_map.html)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](#contributing)


Reference C++ implementations of early **parametric‑map models** used by [FireVoxel](https://firevoxel.org/) to analyze dynamic (4D) medical images such as DCE‑MRI, CT, PET, and SPECT. These models underpin FireVoxel’s **Dynamic Analysis → Calculate Parametric Map** workflow and are shared here for transparency, education, and community contributions.

> **Status:** Initial drop with two foundational models.
>
> • `Model0.cpp` — **Model 0: Signal measurements**  
> • `Model1.cpp` — **Model 1: Signal intensity**  
>   (Model numbering matches the FireVoxel documentation.)

---

## Table of contents

- [What is this repository?](#what-is-this-repository)
- [Repository layout](#repository-layout)
- [Use with FireVoxel (no coding required)](#use-with-firevoxel-no-coding-required)
- [Background: Parametric models](#background-parametric-models)
- [Example datasets & tutorials](#example-datasets--tutorials)
- [Build locally (optional, for contributors)](#build-locally-optional-for-contributors)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [Citing this work](#citing-this-work)
- [Research-use notice](#research-use-notice)
- [Acknowledgments](#acknowledgments)

---

## What is this repository?

This repo hosts compact C++ sources that illustrate how FireVoxel implements parametric analysis for dynamic imaging. The files mirror the numbering used inside FireVoxel’s **Calculate Parametric Map** dialog (e.g., *Model 0, Model 1, …*), making it easy to cross‑reference algorithms with the user documentation and the application UI.

If you only use FireVoxel’s UI, you don’t need to build this code—see the section below for point‑and‑click usage.

---

## Repository layout

```
.
├── Model0.cpp   # Model 0 — Signal measurements
└── Model1.cpp   # Model 1 — Signal intensity
```

Model naming follows the FireVoxel documentation’s **Models** page. See: [https://firevoxel.org/docs/html/userguide/models.html](https://firevoxel.org/docs/html/userguide/models.html).

---

## Use with FireVoxel (no coding required)

You can compute parametric maps directly in FireVoxel:

1. **Open** your dynamic (4D) series in FireVoxel.  
2. Go to **Dynamic Analysis → Calculate Parametric Map**.  
3. Choose a **Model** from the dropdown (only models compatible with your data are shown).  
4. Configure inputs/outputs (e.g., AIF/IDIF for DCE‑MRI models when applicable) and run.

– Quick reference: Parametric Map dialog → https://firevoxel.org/docs/html/userguide/param_map.html  
– DCE‑MRI models (Tofts, extended Tofts, etc.) → https://firevoxel.org/docs/html/userguide/dce_models.html  
– T2/T2* mapping example (Model 16) → https://firevoxel.org/docs/html/userguide/t2map.html

---

## Background: Parametric models

FireVoxel provides a broad family of models under **Dynamic Analysis → Calculate Parametric Map**, spanning descriptive signal models through physiologic compartment models. Representative entries include:

- **Models 0–1:** Descriptive signal measurements/intensity (foundational utilities).  
- **Models 8–9:** **Tofts** and **extended Tofts** (k<sub>trans</sub>, v<sub>e</sub>[, v<sub>p</sub>]); widely used in DCE‑MRI.  
- **2CXM variants:** Two‑compartment exchange models.  

Only models compatible with the current dataset are shown; compatibility is selected automatically based on DICOM header information. See the **Models** page for the full catalog: https://firevoxel.org/docs/html/userguide/models.html.

---

## Example datasets & tutorials

- **Tutorial 9: Introduction to Dynamic Modeling** — load 4D data, manipulate ROIs, generate parametric maps. Videos & data: https://firevoxel.org/video_tutorials/  
- **FireVoxel user documentation** — overview & navigation: https://firevoxel.org/docs/html/index.html

---

## Build locally (optional, for contributors)

These sources are small, self‑contained examples intended for study and extension. They are **not** a stand‑alone application. If you want to experiment (e.g., unit tests, benchmarking), drop the files into your own C++ project (MSVC/Clang/GCC). FireVoxel itself is a Windows research application; typical development uses a Windows C++ toolchain.

> **Tip:** Keep file names aligned with FireVoxel’s internal model IDs (`Model{N}.cpp`) so cross‑referencing to the docs remains straightforward.

---

## Roadmap

- [ ] Add more models (e.g., transit, dual‑input liver, 2CXM variants)  
- [ ] Provide tiny CLI harness & tests for regression checks  
- [ ] Document per‑model inputs/outputs (units, bounds, typical defaults)  
- [ ] Add examples for AIF/IDIF handling where applicable

---

## Contributing

Contributions are welcome! Ideas include porting additional models from the docs, adding tests with synthetic curves, clarifying parameter conventions, or improving comments and numerical stability.

1. Fork the repo and create a feature branch.  
2. Add tests or example scripts if you change model behavior.  
3. Open a PR with a concise description and references (doc section, paper, etc.).

---

## Citing this work

If this repository or FireVoxel helps your research, consider citing:

- **FireVoxel documentation** (main portal and model pages): https://firevoxel.org/docs/html/index.html  
- Mikheev A., Rusinek H. *FireVoxel: Interactive Software for Multi‑Modality Analysis of Medical Images.* J Imaging Inform Med, 2025. PubMed: https://pubmed.ncbi.nlm.nih.gov/39900865/

---

## Research‑use notice

FireVoxel is provided **for research purposes only** and is **not** intended for clinical use. Please consult the user manual and your local regulations before using parametric maps for clinical decision‑making. See **Terms of Use & License**: https://firevoxel.org/docs/html/userguide/intro.html#terms-of-use-license

---

## Acknowledgments

FireVoxel is developed at NYU (Center for Advanced Imaging Innovation and Research). Many features—including dynamic parametric modeling—are described in the public documentation and tutorials.
