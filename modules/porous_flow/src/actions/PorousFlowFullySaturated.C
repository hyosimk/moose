//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "PorousFlowFullySaturated.h"

#include "FEProblem.h"
#include "Conversion.h"
#include "libmesh/string_to_enum.h"

registerMooseAction("PorousFlowApp", PorousFlowFullySaturated, "add_user_object");

registerMooseAction("PorousFlowApp", PorousFlowFullySaturated, "add_kernel");

registerMooseAction("PorousFlowApp", PorousFlowFullySaturated, "add_material");

registerMooseAction("PorousFlowApp", PorousFlowFullySaturated, "add_aux_variable");

registerMooseAction("PorousFlowApp", PorousFlowFullySaturated, "add_aux_kernel");

template <>
InputParameters
validParams<PorousFlowFullySaturated>()
{
  InputParameters params = validParams<PorousFlowSinglePhaseBase>();
  params.addClassDescription(
      "Adds Kernels and fluid-property Materials necessary to simulate a "
      "single-phase fully-saturated flow problem.  Full-upwinding of fluid flow is not available "
      "in this Action, so the results may differ slightly from the Unsaturated Action.  However KT "
      "stabilization may be employed for both the fluid and any heat flow.  No Kernels for "
      "diffusion and dispersion of "
      "fluid components are added.  To run a simulation you will also "
      "need to provide various other Materials for each mesh "
      "block, depending on your simulation type, viz: permeability, "
      "porosity, elasticity tensor, strain calculator, stress calculator, "
      "matrix internal energy, thermal conductivity, diffusivity");
  return params;
}

PorousFlowFullySaturated::PorousFlowFullySaturated(const InputParameters & params)
  : PorousFlowSinglePhaseBase(params)
{
  _objects_to_add.push_back("PorousFlowFullySaturatedDarcyFlow");
  if (_simulation_type == SimulationTypeChoiceEnum::TRANSIENT)
    _objects_to_add.push_back("PorousFlowMassTimeDerivative");
  if ((_coupling_type == CouplingTypeEnum::HydroMechanical ||
       _coupling_type == CouplingTypeEnum::ThermoHydroMechanical) &&
      _simulation_type == SimulationTypeChoiceEnum::TRANSIENT)
    _objects_to_add.push_back("PorousFlowMassVolumetricExpansion");
  if (_coupling_type == CouplingTypeEnum::ThermoHydro ||
      _coupling_type == CouplingTypeEnum::ThermoHydroMechanical)
    _objects_to_add.push_back("PorousFlowFullySaturatedHeatAdvection");
  if (_stabilization == StabilizationEnum::KT)
    _objects_to_add.push_back("PorousFlowAdvectiveFluxCalculatorSaturatedMultiComponent");
  if (_stabilization == StabilizationEnum::KT &&
      (_coupling_type == CouplingTypeEnum::ThermoHydro ||
       _coupling_type == CouplingTypeEnum::ThermoHydroMechanical))
    _objects_to_add.push_back("PorousFlowAdvectiveFluxCalculatorSaturatedHeat");
}

void
PorousFlowFullySaturated::act()
{
  PorousFlowSinglePhaseBase::act();

  // add the kernels
  if (_current_task == "add_kernel")
  {
    if (_stabilization == StabilizationEnum::Full)
    {
      const std::string kernel_type = "PorousFlowFullySaturatedDarcyFlow";
      InputParameters params = _factory.getValidParams(kernel_type);
      params.set<UserObjectName>("PorousFlowDictator") = _dictator_name;
      params.set<RealVectorValue>("gravity") = _gravity;

      for (unsigned i = 0; i < _num_mass_fraction_vars; ++i)
      {
        const std::string kernel_name = "PorousFlowFullySaturated_DarcyFlow" + Moose::stringify(i);
        params.set<unsigned int>("fluid_component") = i;
        params.set<NonlinearVariableName>("variable") = _mass_fraction_vars[i];
        _problem->addKernel(kernel_type, kernel_name, params);
      }
      const std::string kernel_name =
          "PorousFlowFullySaturated_DarcyFlow" + Moose::stringify(_num_mass_fraction_vars);
      params.set<unsigned int>("fluid_component") = _num_mass_fraction_vars;
      params.set<NonlinearVariableName>("variable") = _pp_var;
      _problem->addKernel(kernel_type, kernel_name, params);
    }
    else if (_stabilization == StabilizationEnum::KT)
    {
      const std::string kernel_type = "PorousFlowFluxLimitedTVDAdvection";
      InputParameters params = _factory.getValidParams(kernel_type);
      params.set<UserObjectName>("PorousFlowDictator") = _dictator_name;

      for (unsigned i = 0; i < _num_mass_fraction_vars; ++i)
      {
        const std::string kernel_name = "PorousFlowFluxLimited_DarcyFlow" + Moose::stringify(i);
        params.set<UserObjectName>("advective_flux_calculator") =
            "PorousFlowFullySaturated_AC_" + Moose::stringify(i);
        params.set<NonlinearVariableName>("variable") = _mass_fraction_vars[i];
        _problem->addKernel(kernel_type, kernel_name, params);
      }
      const std::string kernel_name =
          "PorousFlowFluxLimited_DarcyFlow" + Moose::stringify(_num_mass_fraction_vars);
      params.set<NonlinearVariableName>("variable") = _pp_var;
      params.set<UserObjectName>("advective_flux_calculator") =
          "PorousFlowFullySaturated_AC_" + Moose::stringify(_num_mass_fraction_vars);
      _problem->addKernel(kernel_type, kernel_name, params);
    }
  }
  if (_current_task == "add_kernel" && _simulation_type == SimulationTypeChoiceEnum::TRANSIENT)
  {
    std::string kernel_name = "PorousFlowFullySaturated_MassTimeDerivative";
    std::string kernel_type = "PorousFlowMassTimeDerivative";
    InputParameters params = _factory.getValidParams(kernel_type);
    params.set<UserObjectName>("PorousFlowDictator") = _dictator_name;

    for (unsigned i = 0; i < _num_mass_fraction_vars; ++i)
    {
      kernel_name = "PorousFlowFullySaturated_MassTimeDerivative" + Moose::stringify(i);
      params.set<unsigned int>("fluid_component") = i;
      params.set<NonlinearVariableName>("variable") = _mass_fraction_vars[i];
      _problem->addKernel(kernel_type, kernel_name, params);
    }
    kernel_name =
        "PorousFlowFullySaturated_MassTimeDerivative" + Moose::stringify(_num_mass_fraction_vars);
    params.set<unsigned int>("fluid_component") = _num_mass_fraction_vars;
    params.set<NonlinearVariableName>("variable") = _pp_var;
    _problem->addKernel(kernel_type, kernel_name, params);
  }

  if ((_coupling_type == CouplingTypeEnum::HydroMechanical ||
       _coupling_type == CouplingTypeEnum::ThermoHydroMechanical) &&
      _current_task == "add_kernel" && _simulation_type == SimulationTypeChoiceEnum::TRANSIENT)
  {
    std::string kernel_name = "PorousFlowFullySaturated_MassVolumetricExpansion";
    std::string kernel_type = "PorousFlowMassVolumetricExpansion";
    InputParameters params = _factory.getValidParams(kernel_type);
    params.set<UserObjectName>("PorousFlowDictator") = _dictator_name;
    for (unsigned i = 0; i < _num_mass_fraction_vars; ++i)
    {
      kernel_name = "PorousFlowFullySaturated_MassVolumetricExpansion" + Moose::stringify(i);
      params.set<unsigned>("fluid_component") = i;
      params.set<NonlinearVariableName>("variable") = _mass_fraction_vars[i];
      _problem->addKernel(kernel_type, kernel_name, params);
    }
    kernel_name = "PorousFlowFullySaturated_MassVolumetricExpansion" +
                  Moose::stringify(_num_mass_fraction_vars);
    params.set<unsigned>("fluid_component") = _num_mass_fraction_vars;
    params.set<NonlinearVariableName>("variable") = _pp_var;
    _problem->addKernel(kernel_type, kernel_name, params);
  }

  if ((_coupling_type == CouplingTypeEnum::ThermoHydro ||
       _coupling_type == CouplingTypeEnum::ThermoHydroMechanical) &&
      _current_task == "add_kernel")
  {
    if (_stabilization == StabilizationEnum::Full)
    {
      std::string kernel_name = "PorousFlowFullySaturated_HeatAdvection";
      std::string kernel_type = "PorousFlowFullySaturatedHeatAdvection";
      InputParameters params = _factory.getValidParams(kernel_type);
      params.set<NonlinearVariableName>("variable") = _temperature_var[0];
      params.set<UserObjectName>("PorousFlowDictator") = _dictator_name;
      params.set<RealVectorValue>("gravity") = _gravity;
      _problem->addKernel(kernel_type, kernel_name, params);
    }
    else if (_stabilization == StabilizationEnum::KT)
    {
      const std::string kernel_name = "PorousFlowFullySaturated_HeatAdvection";
      const std::string kernel_type = "PorousFlowFluxLimitedTVDAdvection";
      InputParameters params = _factory.getValidParams(kernel_type);
      params.set<NonlinearVariableName>("variable") = _temperature_var[0];
      params.set<UserObjectName>("PorousFlowDictator") = _dictator_name;
      params.set<UserObjectName>("advective_flux_calculator") = "PorousFlowFullySaturatedHeat_AC";
      _problem->addKernel(kernel_type, kernel_name, params);
    }
  }

  // add Materials
  if (_deps.dependsOn(_objects_to_add, "pressure_saturation_qp") && _current_task == "add_material")
  {
    // saturation is always unity, so is trivially calculated using PorousFlow1PhaseFullySaturated
    std::string material_type = "PorousFlow1PhaseFullySaturated";
    InputParameters params = _factory.getValidParams(material_type);
    std::string material_name = "PorousFlowFullySaturated_1PhaseP_qp";
    params.set<UserObjectName>("PorousFlowDictator") = _dictator_name;
    params.set<std::vector<VariableName>>("porepressure") = {_pp_var};
    params.set<bool>("at_nodes") = false;
    _problem->addMaterial(material_type, material_name, params);
  }
  if (_deps.dependsOn(_objects_to_add, "pressure_saturation_nodal") &&
      _current_task == "add_material")
  {
    std::string material_type = "PorousFlow1PhaseFullySaturated";
    InputParameters params = _factory.getValidParams(material_type);
    std::string material_name = "PorousFlowFullySaturated_1PhaseP";
    params.set<UserObjectName>("PorousFlowDictator") = _dictator_name;
    params.set<std::vector<VariableName>>("porepressure") = {_pp_var};
    params.set<bool>("at_nodes") = true;
    _problem->addMaterial(material_type, material_name, params);
  }

  // add Advective Flux calculator UserObjects, if required
  if (_current_task == "add_user_object" && _stabilization == StabilizationEnum::KT)
  {
    for (unsigned i = 0; i < _num_mass_fraction_vars; ++i)
    {
      const std::string userobject_name = "PorousFlowFullySaturated_AC_" + Moose::stringify(i);
      addAdvectiveFluxCalculatorSaturatedMultiComponent(0, i, true, userobject_name);
    }
    const std::string userobject_name =
        "PorousFlowFullySaturated_AC_" + Moose::stringify(_num_mass_fraction_vars);
    if (_num_mass_fraction_vars == 0)
      addAdvectiveFluxCalculatorSaturated(0, true, userobject_name); // 1 component only
    else
      addAdvectiveFluxCalculatorSaturatedMultiComponent(
          0, _num_mass_fraction_vars, true, userobject_name);

    if (_coupling_type == CouplingTypeEnum::ThermoHydro ||
        _coupling_type == CouplingTypeEnum::ThermoHydroMechanical)
    {
      const std::string userobject_name = "PorousFlowFullySaturatedHeat_AC";
      addAdvectiveFluxCalculatorSaturatedHeat(0, true, userobject_name);
    }
  }

  if (_deps.dependsOn(_objects_to_add, "volumetric_strain_qp") ||
      _deps.dependsOn(_objects_to_add, "volumetric_strain_nodal"))
    addVolumetricStrainMaterial(_coupled_displacements, true);

  if (_deps.dependsOn(_objects_to_add, "relative_permeability_qp"))
    addRelativePermeabilityCorey(false, 0, 0.0, 0.0, 0.0);
  if (_deps.dependsOn(_objects_to_add, "relative_permeability_nodal"))
    addRelativePermeabilityCorey(true, 0, 0.0, 0.0, 0.0);
}
