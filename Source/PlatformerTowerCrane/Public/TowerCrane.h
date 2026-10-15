#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TowerCrane.generated.h"

class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCraneKeyframeEvent, int32, KeyframeIndex);

UENUM(BlueprintType)
enum class ECranePlaybackMode : uint8
{
	Loop,
	PingPong,
	Once
};

UENUM(BlueprintType)
enum class ECraneAxisOrder : uint8
{
	Simultaneous,
	OneAtATime
};

USTRUCT(BlueprintType)
struct PLATFORMERTOWERCRANE_API FCraneKeyframe
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower Crane", meta = (MakeEditWidget = true))
	FVector Location = FVector(500.0f, 0.0f, 200.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower Crane", meta = (ClampMin = "0.0", Units = "Seconds"))
	float WaitTime = 1.0f;
};



USTRUCT(BlueprintType)
struct PLATFORMERTOWERCRANE_API FCranePose
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tower Crane")
	float TurnTableYaw = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tower Crane")
	float TrolleyDistance = 0.0f;

	// 0 is at the top of the mast
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tower Crane")
	float LoadHeight = 0.0f;
};



UCLASS(Blueprintable)
class PLATFORMERTOWERCRANE_API ATowerCrane : public AActor
{
	GENERATED_BODY()

public:
	ATowerCrane();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintCallable, Category = "Tower Crane")
	void SetCraneActive(bool bNewActive);

	UFUNCTION(BlueprintPure, Category = "Tower Crane")
	int32 GetCurrentKeyframe() const { return CurrentKeyframe; }

	UFUNCTION(BlueprintPure, Category = "Tower Crane")
	FCranePose PoseForLocation(const FVector& CraneLocation) const;

	UFUNCTION(BlueprintPure, Category = "Tower Crane")
	FVector LocationForPose(const FCranePose& Pose) const;

	UPROPERTY(BlueprintAssignable, Category = "Tower Crane")
	FOnCraneKeyframeEvent OnKeyframeReached;

	UPROPERTY(BlueprintAssignable, Category = "Tower Crane")
	FOnCraneKeyframeEvent OnKeyframeLeft;

	UFUNCTION(CallInEditor, Category = "Tower Crane | Editor")
	void AddKeyframeAtLoad();

protected:
	virtual void BeginPlay() override;

	// Pivot points
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Tower Crane | Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Tower Crane | Components")
	TObjectPtr<USceneComponent> Mast;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Tower Crane | Components")
	TObjectPtr<USceneComponent> Turntable;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Tower Crane | Components")
	TObjectPtr<USceneComponent> Trolley;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Tower Crane | Components")
	TObjectPtr<USceneComponent> Load;


	// Markers														   
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Tower Crane | Markers")
	TObjectPtr<USceneComponent> LowestLoadPoint;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Tower Crane | Markers")
	TObjectPtr<USceneComponent> JibMinPoint;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Tower Crane | Markers")
	TObjectPtr<USceneComponent> JibMaxPoint;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Tower Crane | Markers")
	TObjectPtr<USceneComponent> TopOfLoad;


	// Meshes														   
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Tower Crane | Components")
	TObjectPtr<UStaticMeshComponent> MastMesh;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Tower Crane | Components")
	TObjectPtr<UStaticMeshComponent> JibMesh;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Tower Crane | Components")
	TObjectPtr<UStaticMeshComponent> TrolleyMesh;

	// Scaled along Z by the code to stretch to the load. Its pivot must be at the top of the mesh
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Tower Crane | Components")
	TObjectPtr<UStaticMeshComponent> CableMesh;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Tower Crane | Components")
	TObjectPtr<UStaticMeshComponent> HookMesh;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Tower Crane | Components")
	TObjectPtr<UStaticMeshComponent> LoadMesh;



	// Settings
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tower Crane | Path")
	TArray<FCraneKeyframe> Keyframes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower Crane | Path")
	ECranePlaybackMode PlaybackMode = ECranePlaybackMode::PingPong;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower Crane | Path")
	ECraneAxisOrder AxisOrder = ECraneAxisOrder::Simultaneous;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tower Crane | Path")
	bool bActive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower Crane | Speed", meta = (ClampMin = "0.0", Units = "DegreesPerSecond"))
	float RotationSpeed = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower Crane | Speed", meta = (ClampMin = "0.0", Units = "CentimetersPerSecond"))
	float TrolleySpeed = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tower Crane | Speed", meta = (ClampMin = "0.0", Units = "CentimetersPerSecond"))
	float LiftSpeed = 100.0f;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Tower Crane | Editor", meta = (ClampMin = "0"))
	int32 PreviewKeyframe = 0;
#endif
private:
	// Calculated based on the in editor markers.
	struct FCraneLimits
	{
		FVector Turntable = FVector::ZeroVector;
		FVector JibMin = FVector::ZeroVector;
		FVector JibMax = FVector::ZeroVector;
		float JibYawOffset = 0.0f;
		float MinReach = 0.0f;
		float MaxReach = 0.0f;
		float LowestLoadHeight = 0.0f;
		float HighestLoadHeight = 0.0f;
	};

	enum class ECraneState : uint8
	{
		Moving,
		Waiting
	};

	void CalculateLimits();
	void SnapKeyframesIntoReach();

	void ApplyPose(const FCranePose& Pose);
	void UpdateCable();

	bool StepPoseTowardsTarget(const FCranePose& Target, float DeltaTime);
	void AdvanceToNextKeyframe();

	FVector ActorToMastSpace(const FVector& CraneLocation) const;
	FVector MastToActorSpace(const FVector& MastLocation) const;

	FCraneLimits Limits;
	bool bHasValidSetup = false;
	FString LastSetupProblem;

	FCranePose CurrentPose;
	FCranePose TargetPose;
	ECraneState State = ECraneState::Waiting;
	int32 CurrentKeyframe = 0;
	int32 PingPongDirection = 1;
	float WaitTimer = 0.0f;
};
