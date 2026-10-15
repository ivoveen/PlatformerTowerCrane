#include "TowerCrane.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogTowerCrane, Log, All);


ATowerCrane::ATowerCrane()
{
	PrimaryActorTick.bCanEverTick = true;

	// Set up components
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mast = CreateDefaultSubobject<USceneComponent>(TEXT("Mast"));
	Mast->SetupAttachment(Root);

	LowestLoadPoint = CreateDefaultSubobject<USceneComponent>(TEXT("LowestLoadPoint"));
	LowestLoadPoint->SetupAttachment(Mast);

	MastMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MastMesh"));
	MastMesh->SetupAttachment(Mast);

	Turntable = CreateDefaultSubobject<USceneComponent>(TEXT("Turntable"));
	Turntable->SetupAttachment(Mast);
	Turntable->SetRelativeLocation(FVector(0.0f, 0.0f, 1000.0f));
	Turntable->SetMobility(EComponentMobility::Movable);

	JibMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("JibMesh"));
	JibMesh->SetupAttachment(Turntable);
	JibMesh->SetMobility(EComponentMobility::Movable);

	JibMinPoint = CreateDefaultSubobject<USceneComponent>(TEXT("JibMinPoint"));
	JibMinPoint->SetupAttachment(Turntable);
	JibMinPoint->SetRelativeLocation(FVector(50.0f, 0.0f, 0.0f));
	JibMinPoint->SetMobility(EComponentMobility::Movable);

	JibMaxPoint = CreateDefaultSubobject<USceneComponent>(TEXT("JibMaxPoint"));
	JibMaxPoint->SetupAttachment(Turntable);
	JibMaxPoint->SetRelativeLocation(FVector(100.0f, 0.0f, 0.0f));
	JibMaxPoint->SetMobility(EComponentMobility::Movable);

	Trolley = CreateDefaultSubobject<USceneComponent>(TEXT("Trolley"));
	Trolley->SetupAttachment(Turntable);
	Trolley->SetMobility(EComponentMobility::Movable);

	TrolleyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrolleyMesh"));
	TrolleyMesh->SetupAttachment(Trolley);
	TrolleyMesh->SetMobility(EComponentMobility::Movable);

	CableMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CableMesh"));
	CableMesh->SetupAttachment(Trolley);
	CableMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CableMesh->SetMobility(EComponentMobility::Movable);

	Load = CreateDefaultSubobject<USceneComponent>(TEXT("Load"));
	Load->SetupAttachment(Trolley);
	Load->SetMobility(EComponentMobility::Movable);

	TopOfLoad = CreateDefaultSubobject<USceneComponent>(TEXT("TopOfLoad"));
	TopOfLoad->SetupAttachment(Load);
	TopOfLoad->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	TopOfLoad->SetMobility(EComponentMobility::Movable);

	HookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookMesh"));
	HookMesh->SetupAttachment(Load);
	HookMesh->SetMobility(EComponentMobility::Movable);

	LoadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LoadMesh"));
	LoadMesh->SetupAttachment(Load);
	LoadMesh->SetMobility(EComponentMobility::Movable);
}

void ATowerCrane::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	CalculateLimits();
	if (!bHasValidSetup || Keyframes.IsEmpty())
	{
		return;
	}

	int32 KeyframeToShow = 0;
#if WITH_EDITORONLY_DATA
	KeyframeToShow = FMath::Clamp(PreviewKeyframe, 0, Keyframes.Num() - 1);
#endif
	CurrentPose = PoseForLocation(Keyframes[KeyframeToShow].Location);
	ApplyPose(CurrentPose);
}

void ATowerCrane::BeginPlay()
{
	Super::BeginPlay();

	CalculateLimits();
	if (!bHasValidSetup || Keyframes.IsEmpty())
	{
		return;
	}

	// Start at the first keyframe and wait there before moving to the next one.
	CurrentKeyframe = 0;
	PingPongDirection = 1;
	CurrentPose = PoseForLocation(Keyframes[0].Location);
	ApplyPose(CurrentPose);
	State = ECraneState::Waiting;
	WaitTimer = 0.0f;
}

void ATowerCrane::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bActive || !bHasValidSetup || Keyframes.Num() < 2)
	{
		return;
	}

	if (State == ECraneState::Moving)
	{
		const bool bArrived = StepPoseTowardsTarget(TargetPose, DeltaTime);
		ApplyPose(CurrentPose);

		if (bArrived)
		{
			State = ECraneState::Waiting;
			WaitTimer = 0.0f;
			OnKeyframeReached.Broadcast(CurrentKeyframe);
		}
		return;
	}

	WaitTimer += DeltaTime;
	if (WaitTimer >= Keyframes[CurrentKeyframe].WaitTime)
	{
		OnKeyframeLeft.Broadcast(CurrentKeyframe);
		AdvanceToNextKeyframe();
	}
}

void ATowerCrane::SetCraneActive(bool bNewActive)
{
	bActive = bNewActive;
}

void ATowerCrane::SnapKeyframesIntoReach()
{
	CalculateLimits();
	if (!bHasValidSetup)
	{
		return;
	}

	Modify();
	for (FCraneKeyframe& Keyframe : Keyframes)
	{
		Keyframe.Location = LocationForPose(PoseForLocation(Keyframe.Location));
	}
}

void ATowerCrane::AddKeyframeAtLoad()
{
	Modify();

	FCraneKeyframe& Keyframe = Keyframes.AddDefaulted_GetRef();
	Keyframe.Location = GetActorTransform().InverseTransformPosition(Load->GetComponentLocation());

#if WITH_EDITORONLY_DATA
	PreviewKeyframe = Keyframes.Num() - 1;
#endif
}

void ATowerCrane::CalculateLimits()
{
	FCraneLimits NewLimits;

	NewLimits.Turntable = Turntable->GetRelativeLocation();
	NewLimits.JibMin = JibMinPoint->GetRelativeLocation();
	NewLimits.JibMax = JibMaxPoint->GetRelativeLocation();
	NewLimits.MinReach = NewLimits.JibMin.Size2D();
	NewLimits.MaxReach = NewLimits.JibMax.Size2D();
	NewLimits.JibYawOffset = FMath::RadiansToDegrees(FMath::Atan2(NewLimits.JibMax.Y, NewLimits.JibMax.X));
	NewLimits.LowestLoadHeight = LowestLoadPoint->GetRelativeLocation().Z - NewLimits.Turntable.Z;
	NewLimits.HighestLoadHeight = FMath::Min(NewLimits.JibMin.Z, NewLimits.JibMax.Z) - TopOfLoad->GetRelativeLocation().Z;

	FString Problem;
	if (NewLimits.MaxReach <= NewLimits.MinReach)
	{
		Problem = TEXT("JibMaxPoint must be further from the mast than JibMinPoint.");
	}
	else if (NewLimits.HighestLoadHeight <= NewLimits.LowestLoadHeight)
	{
		Problem = TEXT("LowestLoadPoint must be below the jib, minus the load height.");
	}

	bHasValidSetup = Problem.IsEmpty();
	Limits = NewLimits;

	// Don't spam the log with the same warning
	if (Problem != LastSetupProblem)
	{
		if (!bHasValidSetup)
		{
			UE_LOG(LogTowerCrane, Warning, TEXT("%s: %s"), *GetName(), *Problem);
		}
		LastSetupProblem = Problem;
	}
}

FCranePose ATowerCrane::PoseForLocation(const FVector& CraneLocation) const
{
	const FVector Offset = ActorToMastSpace(CraneLocation) - Limits.Turntable;

	FCranePose Pose;

	// Straight above or below the axis there is no direction to turn to, so keep the current yaw
	if (Offset.Size2D() > KINDA_SMALL_NUMBER)
	{
		Pose.TurnTableYaw = FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X)) - Limits.JibYawOffset;
	}
	else
	{
		Pose.TurnTableYaw = CurrentPose.TurnTableYaw;
	}

	const float Reach = FMath::Clamp(Offset.Size2D(), Limits.MinReach, Limits.MaxReach);
	Pose.TrolleyDistance = Reach - Limits.MinReach;
	Pose.LoadHeight = FMath::Clamp(Offset.Z, Limits.LowestLoadHeight, Limits.HighestLoadHeight);
	return Pose;
}

FVector ATowerCrane::LocationForPose(const FCranePose& Pose) const
{
	const float Angle = FMath::DegreesToRadians(Pose.TurnTableYaw + Limits.JibYawOffset);
	const float Reach = Limits.MinReach + Pose.TrolleyDistance;
	const FVector Offset(FMath::Cos(Angle) * Reach, FMath::Sin(Angle) * Reach, Pose.LoadHeight);
	return MastToActorSpace(Limits.Turntable + Offset);
}

void ATowerCrane::ApplyPose(const FCranePose& Pose)
{
	Turntable->SetRelativeRotation(FRotator(0.0f, Pose.TurnTableYaw, 0.0f));

	const float JibLength = Limits.MaxReach - Limits.MinReach;
	const float Alpha = JibLength > KINDA_SMALL_NUMBER ? Pose.TrolleyDistance / JibLength : 0.0f;
	FVector TrolleyLocation = FMath::Lerp(Limits.JibMin, Limits.JibMax, Alpha);
	TrolleyLocation.Z = 0.0f;
	Trolley->SetRelativeLocation(TrolleyLocation);

	FVector LoadLocation = Load->GetRelativeLocation();
	LoadLocation.Z = Pose.LoadHeight - TrolleyLocation.Z;
	Load->SetRelativeLocation(LoadLocation);

	UpdateCable();
}

void ATowerCrane::UpdateCable()
{
	const UStaticMesh* Mesh = CableMesh->GetStaticMesh();
	if (!Mesh)
	{
		return;
	}

	// Length of the cable mesh at scale 1
	const float MeshLength = Mesh->GetBoundingBox().GetSize().Z;
	if (MeshLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float CableLength = FMath::Max(CableMesh->GetRelativeLocation().Z - Load->GetRelativeLocation().Z, 0.0f);

	FVector Scale = CableMesh->GetRelativeScale3D();
	Scale.Z = CableLength / MeshLength;
	CableMesh->SetRelativeScale3D(Scale);
}

bool ATowerCrane::StepPoseTowardsTarget(const FCranePose& Target, float DeltaTime)
{
	const bool bOneAtATime = AxisOrder == ECraneAxisOrder::OneAtATime;

	CurrentPose.TurnTableYaw = FMath::FixedTurn(CurrentPose.TurnTableYaw, Target.TurnTableYaw, RotationSpeed * DeltaTime);
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentPose.TurnTableYaw, Target.TurnTableYaw);
	const bool bYawDone = FMath::IsNearlyZero(DeltaYaw);
	if (bOneAtATime && !bYawDone)
	{
		return false;
	}

	CurrentPose.TrolleyDistance = FMath::FInterpConstantTo(CurrentPose.TrolleyDistance, Target.TrolleyDistance, DeltaTime, TrolleySpeed);
	const bool bTrolleyDone = FMath::IsNearlyEqual(CurrentPose.TrolleyDistance, Target.TrolleyDistance);
	if (bOneAtATime && !bTrolleyDone)
	{
		return false;
	}

	CurrentPose.LoadHeight = FMath::FInterpConstantTo(CurrentPose.LoadHeight, Target.LoadHeight, DeltaTime, LiftSpeed);
	const bool bLoadDone = FMath::IsNearlyEqual(CurrentPose.LoadHeight, Target.LoadHeight);

	return bYawDone && bTrolleyDone && bLoadDone;
}



void ATowerCrane::AdvanceToNextKeyframe()
{
	const int32 LastKeyframe = Keyframes.Num() - 1;

	switch (PlaybackMode)
	{
	case ECranePlaybackMode::Loop:
		CurrentKeyframe = (CurrentKeyframe + 1) % Keyframes.Num();
		break;

	case ECranePlaybackMode::PingPong:
		if (CurrentKeyframe + PingPongDirection < 0 || CurrentKeyframe + PingPongDirection > LastKeyframe)
		{
			PingPongDirection = -PingPongDirection;
		}
		CurrentKeyframe += PingPongDirection;
		break;

	case ECranePlaybackMode::Once:
		if (CurrentKeyframe == LastKeyframe)
		{
			bActive = false;
			return;
		}
		++CurrentKeyframe;
		break;
	}

	TargetPose = PoseForLocation(Keyframes[CurrentKeyframe].Location);
	State = ECraneState::Moving;
}




FVector ATowerCrane::ActorToMastSpace(const FVector& CraneLocation) const
{
	const FVector WorldLocation = GetActorTransform().TransformPosition(CraneLocation);
	return Mast->GetComponentTransform().InverseTransformPosition(WorldLocation);
}

FVector ATowerCrane::MastToActorSpace(const FVector& MastLocation) const
{
	const FVector WorldLocation = Mast->GetComponentTransform().TransformPosition(MastLocation);
	return GetActorTransform().InverseTransformPosition(WorldLocation);
}




#if WITH_EDITOR
void ATowerCrane::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!Keyframes.IsValidIndex(0)) {
		return;
	}
	if (PropertyChangedEvent.MemberProperty) {
		if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ATowerCrane, Keyframes)) {
			SnapKeyframesIntoReach();
			return;
		}

	}
	if (PropertyChangedEvent.Property) {
		if (PropertyChangedEvent.Property->GetOwnerStruct() == FCraneKeyframe::StaticStruct()) {
			SnapKeyframesIntoReach();
		}
	}
}
#endif
