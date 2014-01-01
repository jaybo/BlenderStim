// DLP4500Init.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "usb.h"
#include "API.h"
#include "LCr4500Init.h"


#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 1

int _tmain(int argc, _TCHAR* argv[])
{
	LCr_Connect();
	return 0;
}


bool LCr_Connect()
{
	char versionStr[255];
	unsigned int API_ver, App_ver, SWConfig_ver, SeqConfig_ver;
	unsigned int FW_ver;
	bool SLmode = 0;

	if (USB_IsConnected())
		USB_Close();
	USB_Open();

	// Display GUI Version #
	sprintf(versionStr, "DLP LightCrafter 4500 Init - %d.%d", APP_VERSION_MAJOR, APP_VERSION_MINOR);
	printf(versionStr);

	if (USB_IsConnected())
	{
		
		if (LCR_GetVersion(&App_ver, &API_ver, &SWConfig_ver, &SeqConfig_ver) == 0)
		{
			sprintf(versionStr, "APIVer:    %d.%d.%d", (API_ver >> 24), ((API_ver << 8) >> 24), ((API_ver << 16) >> 16));
			printf(versionStr);
			sprintf(versionStr, "AppVer:    %d.%d.%d", (App_ver >> 24), ((App_ver << 8) >> 24), ((App_ver << 16) >> 16));
			printf(versionStr);
			sprintf(versionStr, "SWConfig:  %d.%d.%d", (SWConfig_ver >> 24), ((SWConfig_ver << 8) >> 24), ((SWConfig_ver << 16) >> 16));
			printf(versionStr);
			sprintf(versionStr, "SeqConfig: %d.%d.%d", (SeqConfig_ver >> 24), ((SeqConfig_ver << 8) >> 24), ((SeqConfig_ver << 16) >> 16));
			printf(versionStr);
		}
		
		if (LCR_MemRead(0xF902C000, &FW_ver) == 0)
		{
			FW_ver &= 0xFFFFFF;
			sprintf(versionStr, "Firmware:  %d.%d.%d", (FW_ver >> 16), ((FW_ver << 16) >> 24), ((FW_ver << 24) >> 24));
			printf(versionStr);

			// When GUI is first opened, check if old firmware is present & prompt user to upload new version if it is
			/*if (FirstConnection && (FW_ver < RELEASE_FW_VERSION))
			{
			showError("WARNING: Old version of Firmware detected.\n\nDownload the latest release from http://www.ti.com/tool/dlpr350.");
			FirstConnection = FALSE;
			}*/
		}
		//emit on_GetStatusPushButton_clicked();

		//if (LCR_GetMode(&SLmode) == 0)
		//{
		//	if (SLmode)
		//	{
		//		ui->Radiobutton_SLmode->setChecked(true);
		//		ui->RadioButton_videoMode->setChecked(false);
		//	}
		//	else
		//	{
		//		ui->Radiobutton_SLmode->setChecked(false);
		//		ui->RadioButton_videoMode->setChecked(true);
		//	}
		//}
		return true;
	}
	return false;
}


bool LCr_Status()
{
	unsigned char HWStatus, SysStatus, MainStatus;

	if (LCR_GetStatus(&HWStatus, &SysStatus, &MainStatus) == 0)
	{
		if ((HWStatus & BIT0) == BIT0) { //Init Done
			printf("Init Done");
		}
		if ((HWStatus & BIT3) == BIT3) { //Forced Swap
			printf("Forced Swap");
		}
		if ((HWStatus & BIT6) == BIT6) { //Sequence Abort
			printf("Sequence Abort");
		}
		if ((HWStatus & BIT2) == BIT2) { // DRC Error
			printf("DRC Error");
		}
		if ((HWStatus & BIT7) == BIT7) { // Sequence Error
			printf("Sequence Error");
		}

		if ((MainStatus & BIT0) == BIT0) { //DMD Parked
			printf("DMD Parked");
		}
		if ((MainStatus & BIT1) == BIT1){ // Sequence Running
			printf("Sequence Running");
		}
		if ((MainStatus & BIT2) == BIT2){ //Buffer frozen
			printf("Buffer frozen");
		}
		return true;
	}
	return false;
}

void LCr_Reset()
{
	LCR_SoftwareReset();
}

#ifdef foo
void MainWindow::on_SourceSetBut_clicked()
{
	unsigned int index = ui->InputSourceList->currentIndex();
	if (index == 0 || index == 3)
	{
		LCR_SetInputSource(ui->InputSourceList->currentIndex(), ui->InputSourceOptionList->currentIndex());
	}
	else if (index == 1)
	{
		LCR_SetInputSource(ui->InputSourceList->currentIndex(), 0);
		LCR_SetTPGSelect(ui->InputSourceOptionList->currentIndex());
	}
	else
	{
		unsigned int source, portWidth;

		LCR_GetInputSource(&source, &portWidth);
		if (source != 2)
			LCR_SetInputSource(ui->InputSourceList->currentIndex(), 0);
		LCR_LoadSplash(ui->InputSourceOptionList->currentIndex());
	}
}


void MainWindow::on_sendLUT_pushButton_clicked()
{
	int i, numLutEntries = 0;
	unsigned int status;
	char errorStr[256];
	unsigned char splashLut[64];
	int numSplashLutEntries = 0;
	int num_pats_in_exposure = 1;
	unsigned int min_pat_exposure[8] = { 235, 700, 1570, 1700, 2000, 2500, 4500, 8333 };
	unsigned int worstCaseBitDepth = 0;
	unsigned int numPatterns;
	unsigned int total_exposure_time = 0;
	unsigned int rogueExposureTime = 0;
	bool displayTimeLoadTimeMismatch = false;

	if (ui->Seq_listWidget->count() == 0)
		return;

	LCR_ClearPatLut();
	for (i = 0; i<ui->Seq_listWidget->count(); i++)
	{
		if (ui->Seq_listWidget->item(i)->icon().isNull()) //only if pattern item (skip trigger items)
		{
			if (i == 0)
			{
				showError("First Item must be triggered. Please select a Trigger_In_Type other than No Trigger");
				return;
			}

			/*********************Expsosure time validation logic begin *****************/
			//If trigOut = NEW
			if (ui->Seq_listWidget->item(i)->data(Qt::UserRole + trigOutPrev).toBool() == false)
			{
				if (num_pats_in_exposure != 1)
				{
					//Check if expsoure time is above the minimum requirement
					if (ui->patExp_lineEdit->text().toUInt() / num_pats_in_exposure < min_pat_exposure[worstCaseBitDepth])
					{
						sprintf(errorStr, "Exposure time %d < Minimum Exposure time %d for bit depth %d", ui->patExp_lineEdit->text().toUInt() / num_pats_in_exposure, min_pat_exposure[worstCaseBitDepth], worstCaseBitDepth + 1);
						showError(errorStr);
						return;
					}
				}
				if (ui->Seq_listWidget->item(i)->data(Qt::UserRole + BitDepth).toUInt() - 1 > worstCaseBitDepth)
				{
					worstCaseBitDepth = ui->Seq_listWidget->item(i)->data(Qt::UserRole + BitDepth).toInt() - 1;
				}
				num_pats_in_exposure = 1;
				worstCaseBitDepth = 0;
			}
			else //if trigOut = PREV
			{
				num_pats_in_exposure++;
				if (ui->Seq_listWidget->item(i)->data(Qt::UserRole + BitDepth).toUInt() - 1 > worstCaseBitDepth)
					worstCaseBitDepth = ui->Seq_listWidget->item(i)->data(Qt::UserRole + BitDepth).toInt() - 1;
			}

			if (ui->Seq_listWidget->item(i)->data(Qt::UserRole + BufSwap).toBool() == true)
			{
				if (total_exposure_time != 0)
				{
					if (patFromSplash)
					{
						if (total_exposure_time < ui->framePer_lineEdit->text().toUInt())
						{
							displayTimeLoadTimeMismatch = true;
							rogueExposureTime = total_exposure_time;
						}
					}
					else
					{
						if (total_exposure_time > ui->framePer_lineEdit->text().toUInt())
						{
							sprintf(errorStr, "Buffer display time %d > Vsync period %d. Please correct it and try again", numLutEntries*ui->patExp_lineEdit->text().toUInt(), ui->framePer_lineEdit->text().toUInt());
							showError(errorStr);
							return;
						}

					}
				}
				total_exposure_time = ui->patExp_lineEdit->text().toUInt();
			}
			else
			{
				total_exposure_time += ui->patExp_lineEdit->text().toUInt();
			}

			/*********************Expsosure time validation logic End *****************/

			if (LCR_AddToPatLut(ui->Seq_listWidget->item(i)->data(Qt::UserRole + TrigType).toInt(), \
				ui->Seq_listWidget->item(i)->data(Qt::UserRole + PatNum).toInt(), \
				ui->Seq_listWidget->item(i)->data(Qt::UserRole + BitDepth).toInt(), \
				ui->Seq_listWidget->item(i)->data(Qt::UserRole + LEDSelect).toInt(), \
				ui->Seq_listWidget->item(i)->data(Qt::UserRole + InvertPat).toBool(), \
				ui->Seq_listWidget->item(i)->data(Qt::UserRole + InsertBlack).toBool(), \
				ui->Seq_listWidget->item(i)->data(Qt::UserRole + BufSwap).toBool(), \
				ui->Seq_listWidget->item(i)->data(Qt::UserRole + trigOutPrev).toBool()) < 0)
			{
				showError("Error Updating LUT");
				return;
			}
			//If there is a buffer swap or if this is the first pattern
			if ((ui->Seq_listWidget->item(i)->data(Qt::UserRole + BufSwap).toBool()) || (numSplashLutEntries == 0))
			{
				if (numSplashLutEntries >= 64)
					showError("Image LUT entries(64) reached maximum. Will not add anymore entries\n");
				else
					splashLut[numSplashLutEntries++] = ui->Seq_listWidget->item(i)->data(Qt::UserRole + frameIndex).toInt();
			}
			numLutEntries++;
		}
	}

	if (total_exposure_time != 0)
	{
		if (patFromSplash)
		{
			if (total_exposure_time < ui->framePer_lineEdit->text().toUInt())
			{
				displayTimeLoadTimeMismatch = true;
				rogueExposureTime = total_exposure_time;
			}
		}
		else
		{
			if (total_exposure_time > ui->framePer_lineEdit->text().toUInt())
			{
				sprintf(errorStr, "Buffer display time %d > Vsync period %d. Please correct it and try again.", numLutEntries*ui->patExp_lineEdit->text().toUInt(), ui->framePer_lineEdit->text().toUInt());
				showError(errorStr);
				return;
			}

		}
	}

	if (displayTimeLoadTimeMismatch == true && (numSplashLutEntries > 2))
	{
		sprintf(errorStr, "Warning!!! - Buffer display time %d < Buffer load time %d", rogueExposureTime, ui->framePer_lineEdit->text().toUInt());
		showError(errorStr);
	}

	if (numExtraSplashLutEntries)
	{
		for (i = 0; i < numExtraSplashLutEntries; i++)
		{
			if (numSplashLutEntries >= 64)
			{
				showError("Image LUT entries(64) reached maximum. Will not add anymore entries\n");
				break;
			}
			splashLut[numSplashLutEntries++] = extraSplashLutEntries[i];
		}
	}

	if (num_pats_in_exposure != 1)
	{
		//Check if expsoure time is above the minimum requirement
		if (ui->patExp_lineEdit->text().toUInt() / num_pats_in_exposure < min_pat_exposure[worstCaseBitDepth])
		{
			sprintf(errorStr, "Exposure time %d < Minimum Exposure time %d for bit depth %d", ui->patExp_lineEdit->text().toUInt() / num_pats_in_exposure, min_pat_exposure[worstCaseBitDepth], worstCaseBitDepth + 1);
			showError(errorStr);
			return;
		}
	}

	LCR_SetPatternDisplayMode(ui->patFrom_video->isChecked());

	//if play once is selected
	if (ui->patDispMode_radioButton->isChecked())
	{
		numPatterns = numLutEntries;
	}
	else
	{
		numPatterns = ui->PatNum_spinBox->value();
	}

	if (LCR_SetPatternConfig(numLutEntries, ui->patDispMode_radioButton_1->isChecked(), numPatterns, numSplashLutEntries) < 0)
	{
		showError("Error Sending Pattern Config");
		return;
	}
	if (LCR_SetExposure_FramePeriod(ui->patExp_lineEdit->text().toInt(), ui->trigPeriod_lineEdit->text().toInt()) < 0)
	{
		showError("Error Sending Exposure period");
		return;
	}
	if (LCR_SetPatternTriggerMode(ui->IntExtTrigMode_But->isChecked()) < 0)
	{
		showError("Error Sending trigger Mode");
		return;
	}

	if (LCR_SendPatLut() < 0)
	{
		showError("Error Sending Pattern LUT");
		return;
	}
	if (LCR_SendSplashLut(&splashLut[0], numSplashLutEntries) < 0)
	{
		showError("Error Sending Image LUT");
		return;
	}
	if (LCR_ValidatePatLutData(&status) < 0)
	{
		showError("Error validating LUT data");
		return;
	}
	else
	{
		ui->IndicatorButton_17->setEnabled((status & BIT0) == BIT0);
		ui->IndicatorButton_18->setEnabled((status & BIT1) == BIT1);
		ui->IndicatorButton_19->setEnabled((status & BIT2) == BIT2);
		ui->IndicatorButton_20->setEnabled((status & BIT3) == BIT3);
		ui->IndicatorButton_21->setEnabled((status & BIT4) == BIT4);
	}
	if (LCR_PatternDisplay(2) < 0) //Start pattern display
	{
		showError("Error starting pattern display");
		return;
	}

}
#endif