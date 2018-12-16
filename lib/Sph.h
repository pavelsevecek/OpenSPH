/// \file Sph.h
/// \brief Includes common headers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gravity/NBodySolver.h"
#include "io/Column.h"
#include "io/FileSystem.h"
#include "io/LogFile.h"
#include "io/Output.h"
#include "objects/finders/NeighbourFinder.h"
#include "objects/geometry/Domain.h"
#include "physics/Eos.h"
#include "physics/Functions.h"
#include "quantities/Quantity.h"
#include "quantities/QuantityIds.h"
#include "quantities/Storage.h"
#include "run/IRun.h"
#include "sph/Materials.h"
#include "sph/boundary/Boundary.h"
#include "sph/equations/EquationTerm.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Presets.h"
#include "sph/kernel/Kernel.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/StandardSets.h"
#include "system/ArgsParser.h"
#include "system/Factory.h"
#include "timestepping/TimeStepping.h"
