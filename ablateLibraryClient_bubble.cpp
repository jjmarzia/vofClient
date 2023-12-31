#include <string>
#include <memory>
#include "environment/runEnvironment.hpp"
#include "mathFunctions/functionFactory.hpp"
#include "builder.hpp"
#include "domain/boxMesh.hpp"
#include "domain/modifiers/distributeWithGhostCells.hpp"
#include "domain/modifiers/ghostBoundaryCells.hpp"
#include "eos/perfectGas.hpp"
#include "io/interval/fixedInterval.hpp"
#include "monitors/timeStepMonitor.hpp"
#include "parameters/mapParameters.hpp"
#include "utilities/petscUtilities.hpp"
#include "io/hdf5MultiFileSerializer.hpp"
#include "finiteVolume/compressibleFlowFields.hpp"
#include "finiteVolume/fieldFunctions/euler.hpp"
#include "finiteVolume/fieldFunctions/compressibleFlowState.hpp"
#include "finiteVolume/boundaryConditions/essentialGhost.hpp"
#include "solver/timeStepper.hpp"
#include "finiteVolume/finiteVolumeSolver.hpp"
#include "finiteVolume/processes/Process.hpp"
#include "finiteVolume/processes/twoPhaseEulerAdvection.hpp"
#include "eos/twoPhase.hpp"
#include "finiteVolume/fluxCalculator/riemannStiff.hpp"
#include "finiteVolume/processes/surfaceForce.hpp"
#include "domain/initializer.hpp"
#include "finiteVolume/fieldFunctions/densityVolumeFraction.hpp"
#include "mathFunctions/fieldFunction.hpp"
#include "domain/field.hpp"
#include "monitors/logs/stdOut.hpp"
#include "io/interval/simulationTimeInterval.hpp"

int main(int argc, char **argv) {
    // initialize petsc and mpi
    ablate::environment::RunEnvironment::Initialize(&argc, &argv);
    ablate::utilities::PetscUtilities::Initialize();
    {

        //environment:
        //title:
        //tagDirectory:
        ablate::parameters::MapParameters runEnvironmentParameters(
                std::map<std::string, std::string>{{"title", "curvature_AirAir_80x80tt_pressureGradient"}, {"tagDirectory", "false"}}
        );
        ablate::environment::RunEnvironment::Setup(runEnvironmentParameters);

        auto eos = std::make_shared<ablate::eos::PerfectGas>(
        ablate::parameters::MapParameters::Create({
            { "gamma", 0 }, { "Rgas", 0 }
        })
        );

        auto eos1 = std::make_shared<ablate::eos::PerfectGas>( //eosAir
            ablate::parameters::MapParameters::Create({
                    { "gamma", 1.4 }, { "Rgas", 287.0 }}
                    )
        );

        auto eos2 = std::make_shared<ablate::eos::PerfectGas>( //eosAir
                ablate::parameters::MapParameters::Create({
                        { "gamma", 1.4 }, { "Rgas", 287.0 }}
                )
        );

        auto eosTwoPhase = std::make_shared<ablate::eos::TwoPhase>(
                eos1, eos2
        );

        //fields:

        auto fields = std::vector<std::shared_ptr<ablate::domain::FieldDescriptor>>{
            std::make_shared<ablate::finiteVolume::CompressibleFlowFields>(eos),

            std::make_shared<ablate::domain::FieldDescription>(
                "densityvolumeFraction", "dvf", ablate::domain::FieldDescription::ONECOMPONENT, ablate::domain::FieldLocation::SOL, ablate::domain::FieldType::FVM),
            std::make_shared<ablate::domain::FieldDescription>(
                "volumeFraction", "vf", ablate::domain::FieldDescription::ONECOMPONENT, ablate::domain::FieldLocation::SOL, ablate::domain::FieldType::FVM),
            std::make_shared<ablate::domain::FieldDescription>(
                "pressure", "p", ablate::domain::FieldDescription::ONECOMPONENT, ablate::domain::FieldLocation::AUX, ablate::domain::FieldType::FVM)};

        auto modifiers = std::vector<std::shared_ptr<ablate::domain::modifiers::Modifier>>{
            std::make_shared<ablate::domain::modifiers::DistributeWithGhostCells>(1),
            std::make_shared<ablate::domain::modifiers::GhostBoundaryCells>("Face Sets")};

        //domain:
        //name:
        //faces:
        //lower:
        //upper:
        //boundary:
        //options:
        //dm_refine:
        //modifiers:
        //fields:
        auto domain = std::make_shared<ablate::domain::BoxMesh>(
            "simpleBoxField",
                                                                fields,
                                                                modifiers,
                                                                std::vector<int>({40, 40}), //faces
                                                                std::vector<double>({-2, 2}), //upper
                                                                std::vector<double>({-2, 2}), //lower
                                                                std::vector<std::string>{"NONE"} /*boundary*/,
                                                                false /*simplex*/,
                                                                ablate::parameters::MapParameters::Create({{"dm_refine", "1"}}
                                                                )
        );

        //io/serializer:
        //interval:

        auto serializer  = std::make_shared<ablate::io::Hdf5MultiFileSerializer>(
                    std::make_shared<ablate::io::interval::SimulationTimeInterval>(0.1)
                    );
//
//         Set up the flow data
        auto parameters = std::make_shared<ablate::parameters::MapParameters>(
                std::map<std::string, std::string>({{ "cfl", ".5" }})
        );

        auto solutionFieldVolumeFractionCFS = std::make_shared<ablate::mathFunctions::SimpleFormula>("(x^2+y^2) < 1 ?  0 : ( (x^2+y^2) < 1.1025  ? 0.166666666666667 : ( (x^2+y^2) < 1.21 ? 0.333333333333333 : ( (x^2+y^2) < 1.3225 ? 0.5 : ( (x^2+y^2) < 1.44 ? 0.666666666666667 : ( (x^2+y^2) < 1.5625 ? 0.833333333333333 : 1.0 )))))");
        auto solutionFieldTemperatureCFS = std::make_shared<ablate::mathFunctions::ConstantValue>(300);
        auto solutionFieldPressureCFS = std::make_shared<ablate::mathFunctions::SimpleFormula>("(x^2+y^2) < 1 ? 1e5+0.251036495228486 : ( (x^2+y^2) < 1.1025  ? 1e5+0.209197079357072 : ( (x^2+y^2) < 1.21 ? 1e5+0.167357663485657 : ( (x^2+y^2) < 1.3225 ? 1e5+0.125518247614243 : ( (x^2+y^2) < 1.44 ? 1e5+0.083678831742829 : ( (x^2+y^2) < 1.5625 ? 1e5+0.041839415871414 : 1e5 )))))");
        auto solutionFieldVelocityCFS = std::make_shared<ablate::mathFunctions::ConstantValue>(0);
        auto volumeFractionCFS = std::make_shared<ablate::mathFunctions::FieldFunction>(
            "volumeFraction",
            solutionFieldVolumeFractionCFS
            );

        auto cfs = std::make_shared<ablate::finiteVolume::fieldFunctions::CompressibleFlowState>(
            eosTwoPhase,
            solutionFieldTemperatureCFS,
            solutionFieldPressureCFS,
            solutionFieldVelocityCFS,
            volumeFractionCFS
            );

        auto initialization = std::make_shared<ablate::domain::Initializer>(std::vector<std::shared_ptr<ablate::mathFunctions::FieldFunction>>{
            std::make_shared<ablate::finiteVolume::fieldFunctions::Euler>(cfs),
            std::make_shared<ablate::finiteVolume::fieldFunctions::DensityVolumeFraction>(cfs),
            volumeFractionCFS
        });


        // timestepper:
        // name:
        // arguments:
        // ts_type:
        // ts_dt:
        // ts_adapt_type:
        // ts_max_time:
        // io/serializer:
        // domain:
        // initialization:
        auto timeStepper = ablate::solver::TimeStepper("theMainTimeStepper",
                                                       domain,
                                                       ablate::parameters::MapParameters::Create({{"ts_type","euler"}, {"ts_adapt_type", "physicsConstrained"}, {"ts_max_time", "1.1"}, {"ts_dt", "1e-2"}}),
                                                       serializer,
                                                       initialization);

        auto solutionFieldWalls = std::make_shared<ablate::mathFunctions::SimpleFormula>("1.1614401858304297, 1.1614401858304297*215250.0, 0.0, 0.0");
        auto solutionFieldDensityVF = std::make_shared<ablate::mathFunctions::SimpleFormula>("1.1614401858304297");
        auto solutionFieldVF = std::make_shared<ablate::mathFunctions::SimpleFormula>("1.0");
        auto boundaryFunctionWalls = std::make_shared<ablate::mathFunctions::FieldFunction>(
            "euler",
            solutionFieldWalls
            );
        auto boundaryFunctionDensityVF = std::make_shared<ablate::mathFunctions::FieldFunction>(
            "densityvolumeFraction",
            solutionFieldDensityVF
        );
//        auto boundaryFunctionDensityVF = std::make_shared<ablate::finiteVolume::fieldFunctions::DensityVolumeFraction>(
//            eosTwoPhase,
//            ablate::domain::Region::ENTIREDOMAIN
//            );
        auto boundaryFunctionVF = std::make_shared<ablate::mathFunctions::FieldFunction>(
            "volumeFraction",
            solutionFieldVF
            );
        auto boundaryConditions = std::vector<std::shared_ptr<ablate::finiteVolume::boundaryConditions::BoundaryCondition> >{
            std::make_shared<ablate::finiteVolume::boundaryConditions::EssentialGhost>(
                "walls",
                std::vector<int>{1, 2, 3, 4},
                boundaryFunctionWalls,
                "",
                false),
            std::make_shared<ablate::finiteVolume::boundaryConditions::EssentialGhost>(
                "densityVF",
                std::vector<int>{1, 2, 3, 4},
                boundaryFunctionDensityVF,
                "",
                false),
            std::make_shared<ablate::finiteVolume::boundaryConditions::EssentialGhost>(
                "vf",
                std::vector<int>{1, 2, 3, 4},
                boundaryFunctionVF,
                "",
                false)
        };

        auto reimannStiffGasGas = std::make_shared<ablate::finiteVolume::fluxCalculator::RiemannStiff>(eos1, eos2);
        auto reimannStiffGasLiquid = std::make_shared<ablate::finiteVolume::fluxCalculator::RiemannStiff>(eos1, eos2);
        auto reimannStiffLiquidGas = std::make_shared<ablate::finiteVolume::fluxCalculator::RiemannStiff>(eos1, eos2);
        auto reimannStiffLiquidLiquid = std::make_shared<ablate::finiteVolume::fluxCalculator::RiemannStiff>(eos1, eos2);

        auto twoPhaseEulerAdvection = std::make_shared<ablate::finiteVolume::processes::TwoPhaseEulerAdvection>(eosTwoPhase,
                                                                                                                parameters,
                                                                                                                reimannStiffGasGas,
                                                                                                                reimannStiffGasLiquid,
                                                                                                                reimannStiffLiquidGas,
                                                                                                                reimannStiffLiquidLiquid);

//        auto surfaceForceSigma = std::make_shared<ablate::finiteVolume::processes::SurfaceForce>(1.125);

        auto processes = std::vector<std::shared_ptr<ablate::finiteVolume::processes::Process> >{
            // GG, GL, LG, LL
            twoPhaseEulerAdvection,
//            surfaceForceSigma
                };

        //

        auto flowSolver = std::make_shared<ablate::finiteVolume::FiniteVolumeSolver>("flow solver", //id, region, options/parameters, processes, boundary conditions
                                                                                     ablate::domain::Region::ENTIREDOMAIN,
                                                                                     nullptr,
                                                                                     processes,
                                                                                     boundaryConditions
                                                                                     );

        auto fixedInterval = std::make_shared<ablate::io::interval::FixedInterval>(1);
        auto stdOut = std::make_shared<ablate::monitors::logs::StdOut>();

        auto monitor = std::make_shared<ablate::monitors::TimeStepMonitor>(
            stdOut,
            fixedInterval
            );

//        // register the flowSolver with the timeStepper

        timeStepper.Register(
            flowSolver,
            {monitor}
            );
        timeStepper.Solve();
    }
//
    ablate::environment::RunEnvironment::Finalize();
    return 0;
}