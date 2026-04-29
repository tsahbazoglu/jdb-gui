# 🦅 Hawk
### High-Precision Java Debugger for Java Professionals

**Hawk** is a lightweight Java debugger designed for agile, visual clarity, and precision. It is a product of the **New Era of Development**, architected through intensive AI-Human collaboration. This tool represents a shift in engineering where human vision and rigorous testing guide AI-powered code generation to create high-performance native tools.

By prioritizing a **C++/Qt core**, **Hawk** offers a lean alternative to resource-heavy IDEs, giving Linux power users a "Hawk-Eye" view of their Java execution.

---

## 🚀 Key Performance Features

### 🦅 The Predator Engine (Auto-File Resolution)
The debugger operates like a hunter. Even with no files loaded, the moment a breakpoint is hit:
* **JDB Analysis**: It analyzes the JDB output and resolves class paths.
* **Automatic Discovery**: It automatically searches across multiple project paths to find the matching source file.
* **Instant Snap**: It snaps the editor to the file, highlights the execution line in green, and centers the view automatically.

### 🔭 Hawk-Eye Search & Navigation
* **Teleport Search**: A high-performance filtering system that allows you to find and open files instantly within massive directory structures.
* **Multi-Path Probing**: The ability to monitor and search through several project root directories simultaneously.

### 🛡️ Eye-Shield Vision Engine
Designed to protect developer vision during long sessions. It features a synchronized contrast pulse that subtly cycles the background and text brightness to reduce the strain caused by static dark-mode screens.

---

## 🛠 Principles & Technology

* **AI-Architected**: This project is built using professional-grade AI prompts, iterative testing, and human architectural guidance. It is a tool for developers who embrace the future of AI-powered engineering.
* **Linux Principal**: Built exclusively for Linux. No compromises were made for cross-platform compatibility, ensuring the best possible performance and JVM socket integration on native Linux systems.
* **JDB Hardened Core**: Features advanced regex parsing to handle output variations across OpenJDK versions and supports persistent "Phases" (Breakpoint Groups) to manage complex debugging contexts.

---

## 🛠️ Compilation & Setup (Linux)
Since **Hawk** is a native Linux tool built with C++ and Qt, Java developers can follow these streamlined steps to build and run the debugger.

### 1. Install System Dependencies
You will need the standard C++ build tools and the Qt5 development libraries.

```bash
# Update package repositories
sudo apt update

# Install C++ compiler and build-essential
sudo apt install build-essential

# Install Qt5 development libraries
# (Use qtbase5-dev for Ubuntu 20.04+; older systems may use qt5-default)
sudo apt install qtbase5-dev libqt5widgets5

```

### 2. Build the Hawk Binary
Navigate to the project root and run the following commands to generate the executable.
```bash

# Remove the temporary files created during the compilation process
make clean

# Generate the Qt project file with required modules
qmake -project "QT += core gui widgets"

# Create the Makefile
qmake hawk.pro

# Compile using all available CPU cores for maximum speed
make -j $(nproc)
```

### 3. Launching the Debugger
Run the generated hawk binary by passing the paths to your Java source directories as arguments.
```bash
./hawk /path/to/java_project_1 /path/to/java_project_2
```

---

## 🦅 The Name
**Hawk** represents the three pillars of this project: **Vision, Speed, and Precision.** It was built to help developers cut through the noise and strike at the heart of complex bugs with hawk-like accuracy.

---
**Vision for the New Era of Development.**
