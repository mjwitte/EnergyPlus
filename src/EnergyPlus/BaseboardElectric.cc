// C++ Headers
#include <cmath>

// ObjexxFCL Headers
#include <ObjexxFCL/FArray.functions.hh>

// EnergyPlus Headers
#include <BaseboardElectric.hh>
#include <DataHVACGlobals.hh>
#include <DataIPShortCuts.hh>
#include <DataLoopNode.hh>
#include <DataSizing.hh>
#include <DataZoneEnergyDemands.hh>
#include <DataZoneEquipment.hh>
#include <General.hh>
#include <GeneralRoutines.hh>
#include <GlobalNames.hh>
#include <InputProcessor.hh>
#include <OutputProcessor.hh>
#include <Psychrometrics.hh>
#include <ReportSizingManager.hh>
#include <ScheduleManager.hh>
#include <UtilityRoutines.hh>

namespace EnergyPlus {

namespace BaseboardElectric {
	// Module containing the routines dealing with the BASEBOARD Electric HEATER
	// component(s).

	// MODULE INFORMATION:  Richard Liesen
	//       DATE WRITTEN   Nov 2001
	//       RE-ENGINEERED  na

	// PURPOSE OF THIS MODULE:
	// Needs description

	// METHODOLOGY EMPLOYED:
	// Needs description, as appropriate

	// REFERENCES: none

	// OTHER NOTES: none

	// USE STATEMENTS:
	// Use statements for data only modules
	// Using/Aliasing
	using namespace DataGlobals;

	// Use statements for access to subroutines in other modules
	using namespace ScheduleManager;

	// Data
	//MODULE PARAMETER DEFINITIONS
	Fstring const cCMO_BBRadiator_Electric( "ZoneHVAC:Baseboard:Convective:Electric" );
	Real64 const SimpConvAirFlowSpeed( 0.5 ); // m/s

	// DERIVED TYPE DEFINITIONS

	//MODULE VARIABLE DECLARATIONS:
	int NumBaseboards( 0 );
	FArray1D_bool MySizeFlag;
	FArray1D_bool CheckEquipName;

	//SUBROUTINE SPECIFICATIONS FOR MODULE BaseboardRadiator

	// Object Data
	FArray1D< BaseboardParams > Baseboard;

	// Functions

	void
	SimElectricBaseboard(
		Fstring const & EquipName,
		int const ActualZoneNum,
		int const ControlledZoneNum,
		Real64 & PowerMet,
		int & CompIndex
	)
	{

		// SUBROUTINE INFORMATION:
		//       AUTHOR         Richard Liesen
		//       DATE WRITTEN   Nov 2001
		//       MODIFIED       na
		//       RE-ENGINEERED  na

		// PURPOSE OF THIS SUBROUTINE:
		// This subroutine simulates the Electric Baseboard units.

		// METHODOLOGY EMPLOYED:
		// na

		// REFERENCES:
		// na

		// Using/Aliasing
		using DataLoopNode::Node;
		using InputProcessor::FindItemInList;
		using DataZoneEnergyDemands::ZoneSysEnergyDemand;
		using General::TrimSigDigits;

		// Locals
		// SUBROUTINE ARGUMENT DEFINITIONS:

		// SUBROUTINE PARAMETER DEFINITIONS:
		// na

		// INTERFACE BLOCK SPECIFICATIONS
		// na

		// DERIVED TYPE DEFINITIONS
		// na

		// SUBROUTINE LOCAL VARIABLE DECLARATIONS:

		int BaseboardNum; // index of unit in baseboard array
		static bool GetInputFlag( true ); // one time get input flag
		Real64 QZnReq; // zone load not yet satisfied

		if ( GetInputFlag ) {
			GetBaseboardInput();
			GetInputFlag = false;
		}

		// Find the correct Baseboard Equipment
		if ( CompIndex == 0 ) {
			BaseboardNum = FindItemInList( EquipName, Baseboard.EquipName(), NumBaseboards );
			if ( BaseboardNum == 0 ) {
				ShowFatalError( "SimElectricBaseboard: Unit not found=" + trim( EquipName ) );
			}
			CompIndex = BaseboardNum;
		} else {
			BaseboardNum = CompIndex;
			if ( BaseboardNum > NumBaseboards || BaseboardNum < 1 ) {
				ShowFatalError( "SimElectricBaseboard:  Invalid CompIndex passed=" + trim( TrimSigDigits( BaseboardNum ) ) + ", Number of Units=" + trim( TrimSigDigits( NumBaseboards ) ) + ", Entered Unit name=" + trim( EquipName ) );
			}
			if ( CheckEquipName( BaseboardNum ) ) {
				if ( EquipName != Baseboard( BaseboardNum ).EquipName ) {
					ShowFatalError( "SimElectricBaseboard: Invalid CompIndex passed=" + trim( TrimSigDigits( BaseboardNum ) ) + ", Unit name=" + trim( EquipName ) + ", stored Unit Name for that index=" + trim( Baseboard( BaseboardNum ).EquipName ) );
				}
				CheckEquipName( BaseboardNum ) = false;
			}
		}

		InitBaseboard( BaseboardNum, ControlledZoneNum );

		QZnReq = ZoneSysEnergyDemand( ActualZoneNum ).RemainingOutputReqToHeatSP;

		// Simulate baseboard
		SimElectricConvective( BaseboardNum, QZnReq );

		PowerMet = Baseboard( BaseboardNum ).Power;

		ReportBaseboard( BaseboardNum );

	}

	void
	GetBaseboardInput()
	{

		// SUBROUTINE INFORMATION:
		//       AUTHOR         Richard Liesen
		//       DATE WRITTEN   Nov 2001
		//       MODIFIED       na
		//       RE-ENGINEERED  na

		// PURPOSE OF THIS SUBROUTINE:
		// This subroutine gets the input for the Baseboard units.

		// METHODOLOGY EMPLOYED:
		// Standard input processor calls.

		// REFERENCES:
		// na

		// Using/Aliasing
		using InputProcessor::GetNumObjectsFound;
		using InputProcessor::GetObjectItem;
		using InputProcessor::VerifyName;
		using InputProcessor::MakeUPPERCase;
		using InputProcessor::SameString;
		using GlobalNames::VerifyUniqueBaseboardName;
		using namespace DataIPShortCuts;

		// Locals
		// SUBROUTINE ARGUMENT DEFINITIONS:
		// na

		// SUBROUTINE PARAMETER DEFINITIONS:
		static Fstring const RoutineName( "GetBaseboardInput: " ); // include trailing blank space

		// INTERFACE BLOCK SPECIFICATIONS
		// na

		// DERIVED TYPE DEFINITIONS
		// na

		// SUBROUTINE LOCAL VARIABLE DECLARATIONS:
		int BaseboardNum;
		int NumConvElecBaseboards;
		int ConvElecBBNum;
		int NumAlphas;
		int NumNums;
		int IOStat;
		static bool ErrorsFound( false ); // If errors detected in input
		bool IsNotOK; // Flag to verify name
		bool IsBlank; // Flag for blank name
		bool errFlag;

		//BASEBOARD HEATER:ELECTRIC:Convective,
		//  A1 , \field Baseboard Name
		//        \required-field
		//  A2 , \field Available Schedule
		//        \required-field
		//       \type object-list
		//       \object-list ScheduleNames
		//  N1 , \field Efficiency of the Coil
		//       \maximum 1.0
		//       \minimum 0.0
		//       \default 1.0
		//  N2 ; \field Nominal Capacity of the Coil
		//       \units W

		cCurrentModuleObject = cCMO_BBRadiator_Electric;

		NumConvElecBaseboards = GetNumObjectsFound( cCurrentModuleObject );

		// Calculate total number of baseboard units
		NumBaseboards = NumConvElecBaseboards;

		Baseboard.allocate( NumBaseboards );
		CheckEquipName.allocate( NumBaseboards );
		CheckEquipName = true;

		if ( NumConvElecBaseboards > 0 ) { //Get the data for cooling schemes
			BaseboardNum = 0;
			for ( ConvElecBBNum = 1; ConvElecBBNum <= NumConvElecBaseboards; ++ConvElecBBNum ) {

				GetObjectItem( cCurrentModuleObject, ConvElecBBNum, cAlphaArgs, NumAlphas, rNumericArgs, NumNums, IOStat, lNumericFieldBlanks, lAlphaFieldBlanks, cAlphaFieldNames, cNumericFieldNames );

				IsNotOK = false;
				IsBlank = false;
				VerifyName( cAlphaArgs( 1 ), Baseboard.EquipName(), BaseboardNum, IsNotOK, IsBlank, trim( cCurrentModuleObject ) + " Name" );
				if ( IsNotOK ) {
					ErrorsFound = true;
					continue;
				}
				VerifyUniqueBaseboardName( trim( cCurrentModuleObject ), cAlphaArgs( 1 ), errFlag, trim( cCurrentModuleObject ) + " Name" );
				if ( errFlag ) {
					ErrorsFound = true;
				}
				++BaseboardNum;
				Baseboard( BaseboardNum ).EquipName = cAlphaArgs( 1 ); // name of this baseboard
				Baseboard( BaseboardNum ).EquipType = MakeUPPERCase( cCurrentModuleObject ); // the type of baseboard-rename change
				Baseboard( BaseboardNum ).Schedule = cAlphaArgs( 2 );
				if ( lAlphaFieldBlanks( 2 ) ) {
					Baseboard( BaseboardNum ).SchedPtr = ScheduleAlwaysOn;
				} else {
					Baseboard( BaseboardNum ).SchedPtr = GetScheduleIndex( cAlphaArgs( 2 ) );
					if ( Baseboard( BaseboardNum ).SchedPtr == 0 ) {
						ShowSevereError( RoutineName + trim( cCurrentModuleObject ) + ": invalid " + trim( cAlphaFieldNames( 2 ) ) + " entered =" + trim( cAlphaArgs( 2 ) ) + " for " + trim( cAlphaFieldNames( 1 ) ) + "=" + trim( cAlphaArgs( 1 ) ) );
						ErrorsFound = true;
					}
				}
				// get inlet node number
				Baseboard( BaseboardNum ).NominalCapacity = rNumericArgs( 1 );
				Baseboard( BaseboardNum ).BaseboardEfficiency = rNumericArgs( 2 );
			}

			if ( ErrorsFound ) {
				ShowFatalError( RoutineName + "Errors found in getting input.  Preceding condition(s) cause termination." );
			}
		}

		for ( BaseboardNum = 1; BaseboardNum <= NumBaseboards; ++BaseboardNum ) {

			// Setup Report variables for the Electric Baseboards
			// CurrentModuleObject='ZoneHVAC:Baseboard:Convective:Electric'
			SetupOutputVariable( "Baseboard Total Heating Energy [J]", Baseboard( BaseboardNum ).Energy, "System", "Sum", Baseboard( BaseboardNum ).EquipName, _, "ENERGYTRANSFER", "BASEBOARD", _, "System" );

			SetupOutputVariable( "Baseboard Total Heating Rate [W]", Baseboard( BaseboardNum ).Power, "System", "Average", Baseboard( BaseboardNum ).EquipName );

			SetupOutputVariable( "Baseboard Electric Energy [J]", Baseboard( BaseboardNum ).ElecUseLoad, "System", "Sum", Baseboard( BaseboardNum ).EquipName, _, "Electric", "HEATING", _, "System" );

			SetupOutputVariable( "Baseboard Electric Power [W]", Baseboard( BaseboardNum ).ElecUseRate, "System", "Average", Baseboard( BaseboardNum ).EquipName );

		}

	}

	void
	InitBaseboard(
		int const BaseboardNum,
		int const ControlledZoneNum
	)
	{

		// SUBROUTINE INFORMATION:
		//       AUTHOR         Richard Liesen
		//       DATE WRITTEN   Nov 2001
		//       MODIFIED       na
		//       RE-ENGINEERED  na

		// PURPOSE OF THIS SUBROUTINE:
		// This subroutine initializes the Baseboard units during simulation.

		// METHODOLOGY EMPLOYED:
		// na

		// REFERENCES:
		// na

		// Using/Aliasing
		using DataLoopNode::Node;
		using DataZoneEquipment::ZoneEquipInputsFilled;
		using DataZoneEquipment::CheckZoneEquipmentList;
		using DataZoneEquipment::ZoneEquipConfig;

		// Locals
		// SUBROUTINE ARGUMENT DEFINITIONS:

		// SUBROUTINE PARAMETER DEFINITIONS:
		// na

		// INTERFACE BLOCK SPECIFICATIONS
		// na

		// DERIVED TYPE DEFINITIONS
		// na

		// SUBROUTINE LOCAL VARIABLE DECLARATIONS:
		int ZoneNode;
		static bool MyOneTimeFlag( true );
		static bool ZoneEquipmentListChecked( false ); // True after the Zone Equipment List has been checked for items
		int Loop;
		static FArray1D_bool MyEnvrnFlag;

		// Do the one time initializations
		if ( MyOneTimeFlag ) {
			// initialize the environment and sizing flags
			MyEnvrnFlag.allocate( NumBaseboards );
			MySizeFlag.allocate( NumBaseboards );
			MyEnvrnFlag = true;
			MySizeFlag = true;

			MyOneTimeFlag = false;

		}

		// need to check all units to see if they are on ZoneHVAC:EquipmentList or issue warning
		if ( ! ZoneEquipmentListChecked && ZoneEquipInputsFilled ) {
			ZoneEquipmentListChecked = true;
			for ( Loop = 1; Loop <= NumBaseboards; ++Loop ) {
				if ( CheckZoneEquipmentList( Baseboard( Loop ).EquipType, Baseboard( Loop ).EquipName ) ) continue;
				ShowSevereError( "InitBaseboard: Unit=[" + trim( Baseboard( Loop ).EquipType ) + "," + trim( Baseboard( Loop ).EquipName ) + "] is not on any ZoneHVAC:EquipmentList.  It will not be simulated." );
			}
		}

		if ( ! SysSizingCalc && MySizeFlag( BaseboardNum ) ) {
			// for each coil, do the sizing once.
			SizeElectricBaseboard( BaseboardNum );

			MySizeFlag( BaseboardNum ) = false;
		}

		// Set the reporting variables to zero at each timestep.
		Baseboard( BaseboardNum ).Energy = 0.0;
		Baseboard( BaseboardNum ).Power = 0.0;
		Baseboard( BaseboardNum ).ElecUseLoad = 0.0;
		Baseboard( BaseboardNum ).ElecUseRate = 0.0;

		// Do the every time step initializations
		ZoneNode = ZoneEquipConfig( ControlledZoneNum ).ZoneNode;
		Baseboard( BaseboardNum ).AirInletTemp = Node( ZoneNode ).Temp;
		Baseboard( BaseboardNum ).AirInletHumRat = Node( ZoneNode ).HumRat;

	}

	void
	SizeElectricBaseboard( int const BaseboardNum )
	{

		// SUBROUTINE INFORMATION:
		//       AUTHOR         Fred Buhl
		//       DATE WRITTEN   February 2002
		//       MODIFIED       August 2013 Daeho Kang, add component sizing table entries
		//       RE-ENGINEERED  na

		// PURPOSE OF THIS SUBROUTINE:
		// This subroutine is for sizing electric baseboard components for which nominal capacities have not been
		// specified in the input.

		// METHODOLOGY EMPLOYED:
		// Obtains flow rates from the zone sizing arrays and plant sizing data. UAs are
		// calculated by numerically inverting the baseboard calculation routine.

		// REFERENCES:
		// na

		// Using/Aliasing
		using namespace DataSizing;
		using ReportSizingManager::ReportSizingOutput;
		using General::RoundSigDigits;

		// Locals
		// SUBROUTINE ARGUMENT DEFINITIONS:

		// SUBROUTINE PARAMETER DEFINITIONS:
		// na

		// INTERFACE BLOCK SPECIFICATIONS
		// na

		// DERIVED TYPE DEFINITIONS
		// na

		// SUBROUTINE LOCAL VARIABLE DECLARATIONS:
		bool IsAutoSize; // Indicator to autosizing nominal capacity
		Real64 NominalCapacityDes; // Design nominal capacity for reporting
		Real64 NominalCapacityUser; // User hard-sized nominal capacity for reporting

		IsAutoSize = false;
		NominalCapacityDes = 0.0;
		NominalCapacityUser = 0.0;

		if ( CurZoneEqNum > 0 ) {
			if ( Baseboard( BaseboardNum ).NominalCapacity == AutoSize ) {
				IsAutoSize = true;
			}
			if ( ! IsAutoSize && ! ZoneSizingRunDone ) { // Simulation continue
				if ( Baseboard( BaseboardNum ).NominalCapacity > 0.0 ) {
					ReportSizingOutput( cCMO_BBRadiator_Electric, Baseboard( BaseboardNum ).EquipName, "User-Specified Nominal Capacity [W]", Baseboard( BaseboardNum ).NominalCapacity );
				}
			} else { // AutoSize or hard-size with design run
				CheckZoneSizing( Baseboard( BaseboardNum ).EquipType, Baseboard( BaseboardNum ).EquipName );
				NominalCapacityDes = CalcFinalZoneSizing( CurZoneEqNum ).DesHeatLoad * CalcFinalZoneSizing( CurZoneEqNum ).HeatSizingFactor;

				if ( IsAutoSize ) {
					Baseboard( BaseboardNum ).NominalCapacity = NominalCapacityDes;
					ReportSizingOutput( cCMO_BBRadiator_Electric, Baseboard( BaseboardNum ).EquipName, "Design Size Nominal Capacity [W]", NominalCapacityDes );
				} else { // hard-sized with sizing data
					if ( Baseboard( BaseboardNum ).NominalCapacity > 0.0 && NominalCapacityDes > 0.0 ) {
						NominalCapacityUser = Baseboard( BaseboardNum ).NominalCapacity;
						ReportSizingOutput( cCMO_BBRadiator_Electric, Baseboard( BaseboardNum ).EquipName, "Design Size Nominal Capacity [W]", NominalCapacityDes, "User-Specified Nominal Capacity [W]", NominalCapacityUser );
						if ( DisplayExtraWarnings ) {
							if ( ( std::abs( NominalCapacityDes - NominalCapacityUser ) / NominalCapacityUser ) > AutoVsHardSizingThreshold ) {
								ShowMessage( "SizeBaseboard: Potential issue with equipment sizing for ZoneHVAC:Baseboard:Convective:Electric=\"" + trim( Baseboard( BaseboardNum ).EquipName ) + "\"." );
								ShowContinueError( "User-Specified Nominal Capacity of " + trim( RoundSigDigits( NominalCapacityUser, 2 ) ) + " [W]" );
								ShowContinueError( "differs from Design Size Nominal Capacity of " + trim( RoundSigDigits( NominalCapacityDes, 2 ) ) + " [W]" );
								ShowContinueError( "This may, or may not, indicate mismatched component sizes." );
								ShowContinueError( "Verify that the value entered is intended and is consistent with other components." );
							}
						}
					}
				}
			}
		}

	}

	void
	SimElectricConvective(
		int const BaseboardNum,
		Real64 const LoadMet
	)
	{
		// SUBROUTINE INFORMATION:
		//       AUTHOR         Richard Liesen
		//       DATE WRITTEN   Nov 2001
		//       RE-ENGINEERED  na

		// PURPOSE OF THIS SUBROUTINE: This subroutine calculates the heat exchange rate
		// in a pure Electricconvective baseboard heater.

		// METHODOLOGY EMPLOYED:
		// Currently this is primarily modified from HW Convective baseboard which has connections to
		//  a water loop and was necessary to calculate temps, flow rates and other things.  This
		//  model might be made more sophisticated and might use some of those data structures in the future
		//  so they are left in place even though this model does not utilize them.

		// REFERENCES:

		// USE STATEMENTS:
		//unused0909    USE DataEnvironment, ONLY: OutBaroPress
		// Using/Aliasing
		using DataLoopNode::Node;
		using Psychrometrics::PsyCpAirFnWTdb;
		using DataHVACGlobals::SmallLoad;

		// Locals
		// SUBROUTINE ARGUMENT DEFINITIONS:

		// SUBROUTINE PARAMETER DEFINITIONS:
		// na

		// INTERFACE BLOCK SPECIFICATIONS
		// na

		// DERIVED TYPE DEFINITIONS
		// na

		// SUBROUTINE LOCAL VARIABLE DECLARATIONS:
		Real64 AirInletTemp;
		Real64 CpAir;
		Real64 AirMassFlowRate;
		Real64 CapacitanceAir;
		Real64 Effic;
		Real64 AirOutletTemp;
		Real64 QBBCap;

		AirInletTemp = Baseboard( BaseboardNum ).AirInletTemp;
		AirOutletTemp = AirInletTemp;
		CpAir = PsyCpAirFnWTdb( Baseboard( BaseboardNum ).AirInletHumRat, AirInletTemp );
		AirMassFlowRate = SimpConvAirFlowSpeed;
		CapacitanceAir = CpAir * AirMassFlowRate;
		// currently only the efficiency is used to calculate the electric consumption.  There could be some
		//  thermal loss that could be accounted for with this efficiency input.
		Effic = Baseboard( BaseboardNum ).BaseboardEfficiency;

		if ( GetCurrentScheduleValue( Baseboard( BaseboardNum ).SchedPtr ) > 0.0 && LoadMet >= SmallLoad ) {

			// if the load exceeds the capacity than the capacity is set to the BB limit.
			if ( LoadMet > Baseboard( BaseboardNum ).NominalCapacity ) {
				QBBCap = Baseboard( BaseboardNum ).NominalCapacity;
			} else {
				QBBCap = LoadMet;
			}

			// this could be utilized somehow or even reported so the data structures are left in place
			AirOutletTemp = AirInletTemp + QBBCap / CapacitanceAir;

			//The Baseboard electric Load is calculated using the efficiency
			Baseboard( BaseboardNum ).ElecUseRate = QBBCap / Effic;

		} else {
			//if there is an off condition the BB does nothing.
			AirOutletTemp = AirInletTemp;
			QBBCap = 0.0;
			Baseboard( BaseboardNum ).ElecUseRate = 0.0;
		}

		Baseboard( BaseboardNum ).AirOutletTemp = AirOutletTemp;
		Baseboard( BaseboardNum ).Power = QBBCap;

	}

	void
	ReportBaseboard( int const BaseboardNum )
	{

		// SUBROUTINE INFORMATION:
		//       AUTHOR         Richard Liesen
		//       DATE WRITTEN   Nov 2001
		//       MODIFIED       na
		//       RE-ENGINEERED  na

		// PURPOSE OF THIS SUBROUTINE: This subroutine

		// METHODOLOGY EMPLOYED:
		// na

		// REFERENCES:
		// na

		// Using/Aliasing
		using DataHVACGlobals::TimeStepSys;

		// Locals
		// SUBROUTINE ARGUMENT DEFINITIONS:

		// SUBROUTINE PARAMETER DEFINITIONS:
		// na

		// INTERFACE BLOCK SPECIFICATIONS
		// na

		// DERIVED TYPE DEFINITIONS
		// na

		// SUBROUTINE LOCAL VARIABLE DECLARATIONS:
		// na

		Baseboard( BaseboardNum ).Energy = Baseboard( BaseboardNum ).Power * TimeStepSys * SecInHour;
		Baseboard( BaseboardNum ).ElecUseLoad = Baseboard( BaseboardNum ).ElecUseRate * TimeStepSys * SecInHour;

	}

} // BaseboardElectric

} // EnergyPlus