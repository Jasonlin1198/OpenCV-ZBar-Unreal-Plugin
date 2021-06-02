// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>

#include "zbar.h" 

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SphereComponent.h"
#include "Engine/TextureRenderTarget2D.h"

//#include "UObject/UObjectGlobals.h"
//#include "GenericPlatform/GenericPlatformMath.h"
//#include "ImageUtils.h"
//#include "Misc/App.h"

#include "OpenCVWebcam.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(OpenCVPlugin, Log, All);

USTRUCT(BlueprintType)
struct FDecodedObject
{
	GENERATED_USTRUCT_BODY()

public: 

	// Type of qr code detected by ZBar
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = OpenCV)
		FString type;

	// Data decoded by qr code 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = OpenCV)
		FString data;

	// Pixel locations of detected image
	std::vector <cv::Point> location;

	// Equals overloader for object comparison
	bool operator==(const FDecodedObject& obj) const
	{
		return type.Equals(obj.type) && data.Equals(obj.data);
	}

};

UCLASS()
class OPENCV_API AOpenCVWebcam : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AOpenCVWebcam();

	// External camera ID
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = OpenCV)
		int cameraID;

	// Container for USceneCapture2D component
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = OpenCV)
		class USceneCaptureComponent2D* sceneCaptureComponent;

	// Current texture to be rendered on
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = OpenCV)
		class UTextureRenderTarget2D* currentTexture;

	// Container for visible static mesh attached to actor
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = OpenCV)
		UStaticMeshComponent* staticMeshComponent;

	// Image texture resolution to be captured by USceneCapture2D component
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = OpenCV)
		int32 resolutionWidth = 1024;

	// Image texture resolution to be captured by USceneCapture2D component
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = OpenCV)
		int32 resolutionHeight = 1024;

	// Array of decoded objects
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = OpenCV )
		TArray<FDecodedObject> decoded;

	// Enable USceneCapture2D component to render texture
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = OpenCV)
		bool captureEnabled;

	// Enable external camera feed
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = OpenCV)
		bool videoEnabled;
	// Enable OpenCV window render
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = OpenCV)
		bool windowEnabled;

	// The current video frame's corresponding texture
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = OpenCV)
		UTexture2D* VideoTexture;

	// The current video frame's corresponding texture
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = OpenCV)
		UTexture2D* VideoTextureRaw;

	// The current video frame's corresponding texture
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = OpenCV)
		UTexture2D* ExternalCameraTexture;

	// Blueprint Event called every time the video frame is updated
	UFUNCTION(BlueprintImplementableEvent, Category = OpenCV)
		void OnNextVideoFrame();


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	// Called when game ends
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:	

	cv::VideoCapture cvVideoCapture;
	cv::Mat currentVideoFrame;
	bool videoOpen;
	bool windowOpen;
	

	cv::Mat captureSceneToMat();
	UTexture2D* convertMatToTexture(cv::Mat& inputImage, int32 imageResolutionWidth, int32 imageResolutionHeight);
	UTexture2D* convertMatToTextureRaw(cv::Mat& inputImage, int32 imageResolutionWidth, int32 imageResolutionHeight);
	bool convertMatToTextureBoth(cv::Mat& inputImage, int32 imageResolutionWidth, int32 imageResolutionHeight);
	UTexture2D* convertExternalCameraToTexture(cv::Mat& inputImage);
	void decode(cv::Mat& inputImage, TArray<FDecodedObject>& decodedObjects);
	void displayBox(cv::Mat& inputImage, TArray<FDecodedObject>& decodedObjects);
	void displayWindow(cv::Mat& inputImage);
	void printToScreen(FString str, FColor color, float duration = 1.0f);
};
