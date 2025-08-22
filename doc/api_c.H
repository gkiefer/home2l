/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2025 Gundolf Kiefer
 *
 *  Home2L is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Home2L is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Home2L. If not, see <https://www.gnu.org/licenses/>.
 *
 */


// This (C++ header) file contains Doxygen comments for the main page.


/** @mainpage
 * This is the documentation of the **C/C++ API** of the <em>Home2L</em> suite.
 * It is meant as a reference for developers of
 * - resource drivers,
 * - new <em>WallClock</em> applets and
 * - new software tools using the *Home2L Resources* library.
 *
 * The main documentation of the *Home2L* project can be found in the
 * [*Home2L Book*](../home2l-book.pdf). Automation rules are best written
 * using the [*Python API*](../home2l-api_python/index.html).
 *
 * To get started, navigate to the [Topics](topics.html) page.
 *
 * Notes
 * -----
 *
 * ### Coding Conventions
 *
 * Obviously, the source files are no pure C, but have a C++ suffix, are compiled
 * with a C++ compiler, and contain classes. However, for portability, efficiency
 * and readability reasons, C++ language features are used only in a limited
 * way. The reason for using C++ at all instead of pure C are the base classes
 * and the UI modules, where operand overloading (base classes) and inheritance
 * with virtual mathods (UI classes) are very helpful to improve the readability
 * of code.
 *
 * For these reasons, the following conventions apply:
 * 1. All C code is C99-compliant and C++-compliant. All C++ code complies to C++98
 *    (the GCC default in GCC 6.1).
 * 2. No classes or templates from standard C++ libraries are used (STL/libstdc++).
 * 3. From C++, only the following syntax constructs are generally used:
 *    - classes with constructors, destructors, methods, either virtual or not
 *    - operators 'new' and 'delete'
 *    - no reference arguments to methods (unless for copy constructors)
 * 4. Only in modules @ref common_strings and @ref common_containers, the
 *    following additional constructs may occur:
 *    - overloaded operator []
 *    - templates (to be used very carefully)
 */
