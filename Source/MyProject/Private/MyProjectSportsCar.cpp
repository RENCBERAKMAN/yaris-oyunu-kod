﻿#include "MyProjectSportsCar.h"
#include "MyProjectSportsWheelFront.h"
#include "MyProjectSportsWheelRear.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SplineComponent.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

AMyProjectSportsCar::AMyProjectSportsCar()
{
	PrimaryActorTick.bCanEverTick = true;

	// Farlar
	BotLeftHeadlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("BotLeftHeadlight"));
	BotLeftHeadlight->SetupAttachment(GetMesh(), FName("FarSocket_Left"));
	BotLeftHeadlight->SetIntensity(40000.f);
	BotLeftHeadlight->SetAttenuationRadius(2000.f);
	BotLeftHeadlight->SetInnerConeAngle(10.f);
	BotLeftHeadlight->SetOuterConeAngle(80.f);
	BotLeftHeadlight->SetCastShadows(false);

	BotRightHeadlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("BotRightHeadlight"));
	BotRightHeadlight->SetupAttachment(GetMesh(), FName("FarSocket_Right"));
	BotRightHeadlight->SetIntensity(40000.f);
	BotRightHeadlight->SetAttenuationRadius(2000.f);
	BotRightHeadlight->SetInnerConeAngle(10.f);
	BotRightHeadlight->SetOuterConeAngle(80.f);
	BotRightHeadlight->SetCastShadows(false);

	// Fizik ve hareket bileşeni
	UChaosWheeledVehicleMovementComponent* Movement = GetChaosVehicleMovement();
	if (!Movement) return;

	Movement->ChassisHeight = 144.0f;
	Movement->DragCoefficient = 0.1f;
	Movement->bLegacyWheelFrictionPosition = true;
	Movement->WheelSetups.SetNum(4);
	Movement->WheelSetups[0].WheelClass = UMyProjectSportsWheelFront::StaticClass(); Movement->WheelSetups[0].BoneName = FName("Phys_Wheel_FL");
	Movement->WheelSetups[1].WheelClass = UMyProjectSportsWheelFront::StaticClass(); Movement->WheelSetups[1].BoneName = FName("Phys_Wheel_FR");
	Movement->WheelSetups[2].WheelClass = UMyProjectSportsWheelRear::StaticClass();  Movement->WheelSetups[2].BoneName = FName("Phys_Wheel_BL");
	Movement->WheelSetups[3].WheelClass = UMyProjectSportsWheelRear::StaticClass();  Movement->WheelSetups[3].BoneName = FName("Phys_Wheel_BR");

	Movement->EngineSetup.MaxTorque = 7000.0f;
	Movement->EngineSetup.MaxRPM = 50000.0f;
	Movement->EngineSetup.EngineIdleRPM = 10000.0f;

	Movement->TransmissionSetup.bUseAutomaticGears = true;
	Movement->TransmissionSetup.bUseAutoReverse = true;
	Movement->TransmissionSetup.FinalRatio = 8.0f;
	Movement->TransmissionSetup.ChangeUpRPM = 50000.0f;
	Movement->TransmissionSetup.ChangeDownRPM = 24000.0f;
	Movement->TransmissionSetup.GearChangeTime = 0.15f;
	Movement->TransmissionSetup.TransmissionEfficiency = 0.95f;
	Movement->TransmissionSetup.ForwardGearRatios = { 5.00f, 2.60f, 1.80f, 1.35f, 1.0f, 0.82f, 0.68f };
	Movement->TransmissionSetup.ReverseGearRatios = { 4.04f };

	Movement->SteeringSetup.SteeringType = ESteeringType::Ackermann;
	Movement->SteeringSetup.AngleRatio = 0.65f;

	// Fizik stabilitesi
	GetMesh()->SetCenterOfMass(FVector(0.f, 0.f, -10.f));
	GetMesh()->SetLinearDamping(0.5f);
	GetMesh()->SetAngularDamping(1.0f);
}
void AMyProjectSportsCar::ApplyThrottle(float Value)
{
	if (auto* Movement = GetChaosVehicleMovement())
	{
		Movement->SetThrottleInput(Value);
	}
}

void AMyProjectSportsCar::ApplySteer(float Value)
{
	if (auto* Movement = GetChaosVehicleMovement())
	{
		Movement->SetSteeringInput(Value);
	}
}

void AMyProjectSportsCar::ApplyBrake(bool bIsBraking)
{
	if (auto* Movement = GetChaosVehicleMovement())
	{
		Movement->SetBrakeInput(bIsBraking ? 1.0f : 0.0f);
	}
}
void AMyProjectSportsCar::BeginPlay()
{
	Super::BeginPlay();
	LastVelocity = FVector::ZeroVector;
	SmoothedTarget = GetActorLocation();

	if (SplineActor)
		SplineComp = SplineActor->FindComponentByClass<USplineComponent>();
}

void AMyProjectSportsCar::AssignSpline(AActor* InSplineActor)
{
	SplineActor = InSplineActor;
	SplineComp = Cast<USplineComponent>(InSplineActor->FindComponentByClass<USplineComponent>());
}

void AMyProjectSportsCar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float TargetSpeed = 270.f;
	const float Speed = GetVelocity().Size();
	const float SplineLength = SplineComp ? SplineComp->GetSplineLength() : 0.f;

	if (Speed < TargetSpeed)
	{
		FVector Force = GetActorForwardVector() * 120000.f;
		GetMesh()->AddForce(Force);
	}

	TimeSinceLastLaneSwitch += DeltaTime;
	TotalRaceTime += DeltaTime;
	ObstacleAvoidCooldown = FMath::Max(ObstacleAvoidCooldown - DeltaTime, 0.f);

	if (!SplineComp) return;
	DistanceAlongSpline += Speed * DeltaTime;

	if (DistanceAlongSpline > SplineLength)
	{
		DistanceAlongSpline -= SplineLength;
		LapCount++;
	}

	const float ClosestKey = SplineComp->FindInputKeyClosestToWorldLocation(GetActorLocation());
	const float PositionAlongSpline = SplineComp->GetDistanceAlongSplineAtSplineInputKey(ClosestKey);
	const float TargetDist = FMath::Clamp(PositionAlongSpline + 5000.f, 0.f, SplineLength - 100.f);

	FVector SplineTarget = SplineComp->GetLocationAtDistanceAlongSpline(TargetDist, ESplineCoordinateSpace::World);
	FVector RightVec = SplineComp->GetRightVectorAtDistanceAlongSpline(TargetDist, ESplineCoordinateSpace::World);
	FVector TargetLoc = SplineTarget + RightVec * LateralOffset;

	SmoothedTarget = FMath::VInterpTo(SmoothedTarget, TargetLoc, DeltaTime, 30.f);
	FVector DesiredDirection = (SmoothedTarget - GetActorLocation()).GetSafeNormal();

	float SteeringInput = FVector::CrossProduct(GetActorForwardVector(), DesiredDirection).Z;
	if (FMath::Abs(SteeringInput) < 0.06f)
		SteeringInput = 0.f;

	for (TActorIterator<AMyProjectSportsCar> It(GetWorld()); It; ++It)
	{
		AMyProjectSportsCar* Other = *It;
		if (Other == this || !Other->SplineComp) continue;

		float MyDist = LapCount * SplineLength + DistanceAlongSpline;
		float OtherDist = Other->LapCount * SplineLength + Other->DistanceAlongSpline;
		float Gap = OtherDist - MyDist;

		if (Gap > 0.f && Gap < MinFollowDistance)
		{
			if (ObstacleAvoidCooldown <= 0.f)
			{
				LateralOffset = (FMath::RandBool()) ? -300.f : 300.f;
				ObstacleAvoidCooldown = 2.f;
			}
		}
	}

	FVector CurrentVelocity = GetVelocity();
	LastVelocity = CurrentVelocity;

	if (CurrentVelocity.Z > 400.f)
	{
		FVector Vel = GetMesh()->GetPhysicsLinearVelocity();
		Vel.Z = FMath::Clamp(Vel.Z, -100.f, 100.f);
		GetMesh()->SetPhysicsLinearVelocity(Vel);
	}

	GetMesh()->SetLinearDamping(0.1f);
	GetMesh()->SetAngularDamping(0.5f);

	FRotator Rot = GetActorRotation();
	if (FMath::Abs(Rot.Roll) > 25.f || FMath::Abs(Rot.Pitch) > 25.f)
		SetActorRotation(FRotator(0.f, Rot.Yaw, 0.f), ETeleportType::TeleportPhysics);

	const float FinalThrottle = 1.0f;
	const float FinalBrake = 0.0f;

	GetChaosVehicleMovement()->SetSteeringInput(FMath::Clamp(SteeringInput * 1.75f, -1.f, 1.f));
	GetChaosVehicleMovement()->SetThrottleInput(FinalThrottle);
	GetChaosVehicleMovement()->SetBrakeInput(FinalBrake);

	UE_LOG(LogTemp, Warning, TEXT("Steer: %.2f | Throttle: %.2f | Speed: %.0f | Lap: %d"), SteeringInput, FinalThrottle, Speed, LapCount);
	UE_LOG(LogTemp, Warning, TEXT("BotSpeed: %.0f"), Speed);
}