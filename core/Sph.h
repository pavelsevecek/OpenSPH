#pragma once

/// \file Sph.h
/// \brief Includes common headers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "gravity/NBodySolver.h"
#include "io/Column.h"
#include "io/FileSystem.h"
#include "io/LogWriter.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "objects/finders/NeighborFinder.h"
#include "objects/geometry/Domain.h"
#include "physics/Eos.h"
#include "physics/Functions.h"
#include "quantities/Quantity.h"
#include "quantities/QuantityIds.h"
#include "quantities/Storage.h"
#include "run/IRun.h"
#include "sph/Materials.h"
#include "sph/boundary/Boundary.h"
#include "sph/equations/Derivative.h"
#include "sph/equations/EquationTerm.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Galaxy.h"
#include "sph/initial/Initial.h"
#include "sph/initial/Stellar.h"
#include "sph/kernel/Interpolation.h"
#include "sph/kernel/Kernel.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/StandardSets.h"
#include "sph/solvers/SymmetricSolver.h"
#include "system/ArgsParser.h"
#include "system/Factory.h"
#include "thread/Pool.h"
#include "timestepping/TimeStepCriterion.h"
#include "timestepping/TimeStepping.h"
