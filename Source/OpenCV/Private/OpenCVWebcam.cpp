// Fill out your copyright notice in the Description page of Project Settings.
#include "OpenCVWebcam.h"

#include <iostream>

using namespace cv;
using namespace std;
using namespace zbar;

DEFINE_LOG_CATEGORY(OpenCVPlugin);


// Sets default values
AOpenCVWebcam::AOpenCVWebcam()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Initialize openCV objects
	cvVideoCapture = VideoCapture();
	currentVideoFrame = Mat();

	// Initialize features exposed to blueprint
	captureEnabled = false;
	videoEnabled = false;
	windowEnabled = false;

	// Internal flags
	videoOpen = false;
	windowOpen = false;
	
	// Attach scenecapture camera to actor and set as root
	sceneCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Scene Capture"));
	sceneCaptureComponent->bCaptureEveryFrame = false;
	// Create Static Mesh Component and attach Cube to camera
	staticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh Component"));
	UStaticMesh* cubeMesh = ConstructorHelpers::FObjectFinder<UStaticMesh>(TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'")).Object;
	staticMeshComponent->SetStaticMesh(cubeMesh);
	staticMeshComponent->AttachToComponent(sceneCaptureComponent, FAttachmentTransformRules::KeepRelativeTransform);
	staticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Set camera to as root
	SetRootComponent(sceneCaptureComponent);
}


// Called when the game starts or when spawned
void AOpenCVWebcam::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(OpenCVPlugin, Warning, TEXT("Begin Play"));

}


// Called every frame
void AOpenCVWebcam::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Open external camera if functionality enabled
	if (videoEnabled && !videoOpen) {
		if (!cvVideoCapture.isOpened()) {
			cvVideoCapture.open(cameraID);
		}
		if (cvVideoCapture.isOpened()) {
			videoOpen = true;
			UE_LOG(OpenCVPlugin, Warning, TEXT("Opened Camera"));
		}
	}

	// Main Driver code for handling OpenCV functionality
	if (captureEnabled) {

		// Capture external webcam image as texture
		if (videoEnabled && videoOpen) {
			Mat inputImage;
			cvVideoCapture.read(inputImage);

			// Set webcam texture into blueprint accessible variable
			ExternalCameraTexture = convertExternalCameraToTexture(inputImage);
		}

		// Ensure that resolution is power of 2, display final resolution in actor properties
		resolutionWidth = FGenericPlatformMath::Pow(2, FGenericPlatformMath::FloorLog2(FGenericPlatformMath::Max(resolutionWidth, 1) * 2 - 1));
		resolutionHeight = FGenericPlatformMath::Pow(2, FGenericPlatformMath::FloorLog2(FGenericPlatformMath::Max(resolutionHeight, 1) * 2 - 1));

		// Capture scene view, process image, and create texture
		Mat inputImage = captureSceneToMat();
		// Check that image has elements
		if (!inputImage.empty()) {
			TArray<FDecodedObject> decodedObjects;

			// Find and decode barcodes and QR codes
			decode(inputImage, decodedObjects);

			// Display location
			displayBox(inputImage, decodedObjects);

			decodedObjects.Empty();

			VideoTexture = convertMatToTexture(inputImage, resolutionWidth, resolutionHeight);
			VideoTextureRaw = convertMatToTextureRaw(inputImage, resolutionWidth, resolutionHeight);
		
		
			if (windowEnabled) {
				windowOpen = true;
				displayWindow(inputImage);
			}
		}
	}
	// Clear array of decoded images when capture is ended
	else {
		decoded.Empty();
	}

	// Call custom event handler function
	OnNextVideoFrame();

}

void AOpenCVWebcam::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// Close camera
	if (videoOpen) {
		videoOpen = false;
		cvVideoCapture.release();
	}
	
	// Close OpenCV window by name
	if (windowOpen) {
		windowOpen = false;
		destroyWindow("QRCode-Zbar");
	}
}

// Display External Camera Feed
UTexture2D* AOpenCVWebcam::convertExternalCameraToTexture(Mat& inputImage) {

	TArray<FColor> pixels;

	int32 imageResolutionWidth = inputImage.cols;
	int32 imageResolutionHeight = inputImage.rows;
	
	pixels.Init(FColor(0, 0, 0, 255), imageResolutionWidth * imageResolutionHeight);

	// Copy Mat data to Data array
	for (int y = 0; y < imageResolutionHeight; y++)
	{
		for (int x = 0; x < imageResolutionWidth; x++)
		{
			int i = x + (y * imageResolutionWidth);
			pixels[i].R = inputImage.data[i * 3 + 0];
			pixels[i].G = inputImage.data[i * 3 + 1];
			pixels[i].B  = inputImage.data[i * 3 + 2];
		}
	}

	// Create raw output texture
	UTexture2D* texture = UTexture2D::CreateTransient(imageResolutionWidth, imageResolutionHeight, EPixelFormat::PF_R8G8B8A8, "External Camera texture");

	if (!texture) {
		UE_LOG(OpenCVPlugin, Error, TEXT("Failed to create camera texture"));
		return nullptr;
	}

	void* data = texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(data, pixels.GetData(), imageResolutionWidth * imageResolutionHeight * 4);
	texture->PlatformData->Mips[0].BulkData.Unlock();
	texture->UpdateResource();
	texture->Filter = TextureFilter::TF_Nearest;

	return texture;
}


// Capture scene view from scene capture component and pass image into OpenCV processing
Mat AOpenCVWebcam::captureSceneToMat() {

	//const FVector2D ViewportSize = FVector2D(GEngine->GameViewport->Viewport->GetSizeXY());
	//int32 imageResolutionWidth = ViewportSize.X;
	//int32 imageResolutionHeight = ViewportSize.Y;

	// Create new texture target
	UTextureRenderTarget2D* currentTarget = NewObject<UTextureRenderTarget2D>();
	currentTarget->InitCustomFormat(resolutionWidth, resolutionHeight, PF_B8G8R8A8, false);

	// Assign to scene capture component
	currentTexture = currentTarget;
	sceneCaptureComponent->TextureTarget = currentTarget;

	// Capture scene texture
	sceneCaptureComponent->CaptureScene();

	// Create container for pixels
	TArray<FColor> imagePixels;
	imagePixels.Empty();
	imagePixels.Reserve(resolutionWidth * resolutionHeight);

	// Get texture render target
	FTextureRenderTargetResource* textureResource = currentTarget->GameThread_GetRenderTargetResource();

	// Populate RGBA8 texture pixels into buffer
	bool readPixelResult = textureResource->ReadPixels(imagePixels);

	// Pixel read unsuccessful
	if (!readPixelResult) {
		UE_LOG(OpenCVPlugin, Warning, TEXT("Failed read pixels from render target resource"));
		//return Mat();
	}

	// Shink array to number of pixels read
	imagePixels.Shrink();

	// Print pixel data to output log
	//for (int32 i = 0; i < imagePixels.Num(); i++) {
	//	UE_LOG(OpenCVPlugin, Warning, TEXT("%s"), *imagePixels[i].ToString());
	//}
	
	// Create array for 8-bit int conversion
	TArray<uint8> pixelData;
	int32 bufferSize = imagePixels.Num() * 4;
	pixelData.Reserve(bufferSize);

	// Copy texture render pixels into array with BGR format OpenCV expects
	for (int i = 0; i < imagePixels.Num(); i++) {
		pixelData.Append({ imagePixels[i].B, imagePixels[i].G, imagePixels[i].R, 0xff });
	}

	// Get pointer to first element of array
	void* imageData = pixelData.GetData();

	// Construct OpenCv image defined by image width, height, 8-bit 4 color channel pixels, and pointer to data
	Mat inputImage = Mat(resolutionWidth, resolutionHeight, CV_8UC4, imageData);

	// Return clone to get valid Mat reference since inputImage is invalid after function return
	return inputImage.clone();
}


// Convert OpenCV image into UE texture2D image
UTexture2D* AOpenCVWebcam::convertMatToTexture(Mat& inputImage, int32 imageResolutionWidth, int32 imageResolutionHeight) {

	TArray<FColor> pixels;

	// Convert OpenCV Mat image into UE formatted pixel arrays
	for (int x = 0; x < imageResolutionWidth; x++) {
		for (int y = 0; y < imageResolutionHeight; y++) {
			// Current Color point
			Vec4b point = inputImage.at<Vec4b>(x, y);

			// Append pure blue pixels, otherwise transparent
			if (point[0] == 255 && point[1] == 0 && point[2] == 0) {
				FColor pix(point[0], point[1], point[2], 0xff);
				pixels.Add(pix);
			}
			else {
				FColor pix(0, 0, 0, 0x00);
				pixels.Add(pix);
			}
		}
	}

	// Create modified texture with RGB encoding format
	UTexture2D* texture = UTexture2D::CreateTransient(imageResolutionWidth, imageResolutionHeight, EPixelFormat::PF_R8G8B8A8, "OpenCV Texture Output");

	if (!texture) {
		UE_LOG(OpenCVPlugin, Error, TEXT("Failed to create output texture"));
		return nullptr;
	}

	void* data = texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(data, pixels.GetData(), imageResolutionWidth * imageResolutionHeight * 4);
	texture->PlatformData->Mips[0].BulkData.Unlock();
	texture->UpdateResource();
	texture->Filter = TextureFilter::TF_Nearest;

	return texture;
}


// Convert OpenCV image into UE texture2D image
UTexture2D* AOpenCVWebcam::convertMatToTextureRaw(Mat& inputImage, int32 imageResolutionWidth, int32 imageResolutionHeight) {

	TArray<FColor> pixels;

	// Convert OpenCV Mat image into UE formatted pixel arrays
	for (int x = 0; x < imageResolutionWidth; x++) {
		for (int y = 0; y < imageResolutionHeight; y++) {
			// Current Color point
			Vec4b point = inputImage.at<Vec4b>(x, y);
			// Append all pixels into raw array
			FColor pix(point[0], point[1], point[2], 0xff);
			pixels.Add(pix);
		}
	}

	// Create modified texture with RGB encoding format
	UTexture2D* texture = UTexture2D::CreateTransient(imageResolutionWidth, imageResolutionHeight, EPixelFormat::PF_R8G8B8A8, "OpenCV Raw Texture Output");

	if (!texture) {
		UE_LOG(OpenCVPlugin, Error, TEXT("Failed to create raw output texture"));
		return nullptr;
	}

	void* data = texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(data, pixels.GetData(), imageResolutionWidth * imageResolutionHeight * 4);
	texture->PlatformData->Mips[0].BulkData.Unlock();
	texture->UpdateResource();
	texture->Filter = TextureFilter::TF_Nearest;

	return texture;
}


// Convert OpenCV image into UE texture2D image
bool AOpenCVWebcam::convertMatToTextureBoth(Mat& inputImage, int32 imageResolutionWidth, int32 imageResolutionHeight) {

	TArray<FColor> pixels;
	TArray<FColor> pixelsRaw;

	// Convert OpenCV Mat image into UE formatted pixel arrays
	for (int x = 0; x < imageResolutionWidth; x++) {
		for (int y = 0; y < imageResolutionHeight; y++) {
			// Current Color point
			Vec4b point = inputImage.at<Vec4b>(x, y);

			// Append pure blue pixels, otherwise transparent
			if (point[0] == 255 && point[1] == 0 && point[2] == 0) {
				FColor pix(point[0], point[1], point[2], 0xff);
				pixels.Add(pix);
			}
			else {
				FColor pix(0, 0, 0, 0x00);
				pixels.Add(pix);
			}
			// Append all pixels into raw array
			FColor pixRaw(point[0], point[1], point[2], 0xff);
			pixelsRaw.Add(pixRaw);
		}
	}

	// Create modified texture with RGB encoding format
	UTexture2D* texture = UTexture2D::CreateTransient(imageResolutionWidth, imageResolutionHeight, EPixelFormat::PF_R8G8B8A8, "OpenCV Texture Output");

	if (!texture) {
		UE_LOG(OpenCVPlugin, Error, TEXT("Failed to create output texture"));
		return false;
	}

	void* data = texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(data, pixels.GetData(), imageResolutionWidth * imageResolutionHeight * 4);
	texture->PlatformData->Mips[0].BulkData.Unlock();
	texture->UpdateResource();
	texture->Filter = TextureFilter::TF_Nearest;

	VideoTexture = texture;

	// Create raw output texture
	UTexture2D* textureRaw = UTexture2D::CreateTransient(imageResolutionWidth, imageResolutionHeight, EPixelFormat::PF_R8G8B8A8, "OpenCV Texture Output Raw");

	if (!textureRaw) {
		UE_LOG(OpenCVPlugin, Error, TEXT("Failed to create raw output texture"));
		return false;
	}

	void* dataRaw = textureRaw->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(dataRaw, pixelsRaw.GetData(), imageResolutionWidth * imageResolutionHeight * 4);
	textureRaw->PlatformData->Mips[0].BulkData.Unlock();
	textureRaw->UpdateResource();
	textureRaw->Filter = TextureFilter::TF_Nearest;

	VideoTextureRaw = textureRaw;

	return true;
}



// Find and decode barcodes and QR codes
void AOpenCVWebcam::decode(Mat& inputImage, TArray<FDecodedObject>& decodedObjects)
{
	// Create zbar scanner
	ImageScanner scanner;

	// Configure scanner to detect image types
	scanner.set_config(ZBAR_NONE, ZBAR_CFG_ENABLE, 1);

	// Convert image to grayscale
	Mat imGray;
	cvtColor(inputImage, imGray, COLOR_BGR2GRAY);

	// Wrap opencv Mat image data in a zbar image
	Image image(inputImage.cols, inputImage.rows, "Y800", (uchar*)imGray.data, inputImage.cols * inputImage.rows);

	// Display grayscale output video
	//im = imGray;

	// Scan the image for barcodes and QRCodes
	int n = scanner.scan(image);

	if (n <= 0) {
		UE_LOG(OpenCVPlugin, Warning, TEXT("No QR Detected"));
		return;
	}

	// Print results
	for (Image::SymbolIterator symbol = image.symbol_begin(); symbol != image.symbol_end(); ++symbol)
	{
		FDecodedObject obj;

		//cout << to_string(symbol->get_location_size()) << endl;

		// Print type and data
		FString type(symbol->get_type_name().c_str());
		FString data(symbol->get_data().c_str());

		obj.type = type;
		obj.data = data;

		// Print data to Log
		UE_LOG(OpenCVPlugin, Warning, TEXT("%s"), *type);
		UE_LOG(OpenCVPlugin, Warning, TEXT("%s"), *data);

		// Print data to screen
		static double deltaTime = FApp::GetDeltaTime();
		printToScreen(obj.type, FColor::Green, deltaTime);
		printToScreen(obj.data, FColor::Blue, deltaTime);

		// Obtain location
		for (int i = 0; i < symbol->get_location_size(); i++)
		{
			obj.location.push_back(Point(symbol->get_location_x(i), symbol->get_location_y(i)));

			// cout << "X: " + to_string(symbol->get_location_x(i)) + " Y: " + to_string(symbol->get_location_y(i)) << endl;

		}

		// Add unique detections to visible property
		decoded.AddUnique(obj);

		// Add all detections for displaying image outlines
		decodedObjects.Add(obj);
	}
}


// Display barcode outline at QR code location
void AOpenCVWebcam::displayBox(Mat& inputImage, TArray<FDecodedObject>& decodedObjects)
{
	// Loop over all decoded objects
	for (int i = 0; i < decodedObjects.Num(); i++)
	{
		vector<Point> points = decodedObjects[i].location;
		vector<Point> hull;

		// If the points do not form a quad, find convex hull
		if (points.size() > 4) {
			convexHull(points, hull);
		}
		else {
			hull = points;
		}

		// Number of points in the convex hull
		int n = hull.size();

		for (int j = 0; j < n; j++)
		{
			line(inputImage, hull[j], hull[(j + 1) % n], Scalar(255, 0, 0), 2);
		}

	}
}


// Display results in external OpenCV Window
void AOpenCVWebcam::displayWindow(Mat& inputImage) {
	imshow("QRCode-Zbar", inputImage);
	waitKey(1);
}

void AOpenCVWebcam::printToScreen(FString str, FColor color, float duration) {
	GEngine->AddOnScreenDebugMessage(-1, duration, color, *str);
}