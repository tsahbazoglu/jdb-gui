![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)
![C++](https://img.shields.io/badge/C++-20-blue?logo=cplusplus)
![Qt](https://img.shields.io/badge/Qt-6-green?logo=qt)

# 🦅 Hawk


## License
This project is licensed under the GNU General Public License v3.0. 
See the [LICENSE](LICENSE) file for the full license text.

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


## 🛠️ Compilation & Setup (Windows)
To build Hawk on Windows, you need the Qt framework and a compatible C++ compiler.

### 1. Prerequisites
Qt Framework: Download the  <a href="https://www.qt.io/development/download-qt-installer" target="_blank" rel="noopener noreferrer">Qt Online Installer</a>. During installation, select:

Qt 5.15.x or Qt 6.x

MinGW (Recommended for simplicity) or MSVC 2019/2022.

CMake/Ninja: Usually included with the Qt installer.

### 2. Build via Command Line (MinGW)
Open the Qt Command Prompt (available in your Start Menu after installation) and run:

### PowerShell
```bash
# 1. Generate the Project file
qmake -project "QT += core gui widgets"
```

# 2. Generate the Makefile
```bash
qmake hawk.pro
```

# 3. Compile the project
```bash
# 'mingw32-make' is the Windows equivalent of 'make'
mingw32-make -j %NUMBER_OF_PROCESSORS%
```

### 3. Build via Qt Creator (Recommended for Windows)
Open Qt Creator.

Go to File > Open File or Project and select your hawk.pro.

Select your Kit (e.g., Desktop Qt 6.x.x MinGW).

Click the Hammer icon (Build) in the bottom left.

Find hawk.exe in your build folder.

### 4. Running the Debugger
Pass your project paths just like on Linux. Note that Windows uses backslashes, but Qt handles both:

```bash
# PowerShell
.\hawk.exe C:\Users\Dev\Project1 D:\JavaSource\Project2
```

### 💡 "Make Clean" for Windows
If you need to reset your build environment on Windows, use:

```bash
# PowerShell

# Removes temporary build artifacts and the .exe
mingw32-make clean
```

---

## 🗺️ Roadmap

The current status and future plans of the project are outlined below. If you would like to contribute, please take a look at the tasks in the **"Planned"** stage!

| Status | Feature | Description | Priority |
| :---: | :--- | :--- | :---: |
| ✅ | **Core JDB Connection** | Basic integration with Java Debugger process. | High |
| ✅ | **Breakpoint Management** | Adding, removing, and listing breakpoints via GUI. | High |
| 🚧 | **Live Line Highlighting** | Real-time visual tracking of the current executing line. | High |
| 📅 | **Variables Inspector** | A dedicated panel to view and modify local variables. | Medium |
| 📅 | **Call Stack View** | Visualizing the current thread's call stack. | Medium |
| 📅 | **Multi-Thread Support** | Switching between different threads and monitoring states. | Low |
| 🚀 | **One-Click Installer** | Pre-built binaries for Windows, Linux, and macOS. | Low |

**Legend:**
*   ✅ **Done:** Completed and available in the stable version.
*   🚧 **In Progress:** Currently under development.
*   📅 **Planned:** Scheduled for future releases.
*   🚀 **Future Goal:** Long-term objectives.
---

**Vision for the New Era of Development.**

## 🦅 Troubleshooting

1. Check the Display Protocol (Wayland vs. X11)
```bash
echo $XDG_SESSION_TYPE
```

2. Check the Desktop Environment (GNOME, KDE, etc.)
```bash
echo $XDG_CURRENT_DESKTOP
```

4. All-in-One
```bash
loginctl show-session $(loginctl | grep $(whoami) | awk '{print $1}') -p Type
```

5. OR the more user-friendly:
```bash
hostnamectl
```
6. Quick Fix for GNOME on Wayland:
If you are on GNOME, your best bet to make Hawk pop up is to disable the protection via terminal:
```bash
gsettings get org.gnome.desktop.wm.preferences focus-new-windows
```

```bash
gsettings set org.gnome.desktop.wm.preferences focus-new-windows 'smart'
```

revert
```bash
gsettings set org.gnome.desktop.wm.preferences focus-new-windows 'strict'
```


## 🦅 The Name
**Hawk** represents the three pillars of this project: **Vision, Speed, and Precision.** It was built to help developers cut through the noise and strike at the heart of complex bugs with hawk-like accuracy.

---









