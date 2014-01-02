/*
 * mainwindow.cpp
 *
 * This module contains the constructor and destructor functions as well as the GUI action related functions.
 *
 * Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com/
 * ALL RIGHTS RESERVED
 *
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMainWindow>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QtGlobal>
#include <QThread>
#include <QTextStream>
#include <QListWidgetItem>
#include <QItemSelectionModel>
#include <QMenu>
#include <QPainter>
#include <QTimer>
#include <QTime>
#include <stdio.h>
#include <stdlib.h>
#include "hidapi.h"
#include "usb.h"
#include "API.h"
#include "string.h"

#include "Helper.h"
#include "FlashDevice.h"
#include "BMPParser.h"
#include "Common.h"
#include "Error.h"
#include "firmware.h"
#include "version.h"

FlashDevice myFlashDevice;
static int FrameIdx=0;
bool patFromSplash=true;
static unsigned char *pImageBuffer;
QString displayStr_splashImageCount = "# Original Images: ";
QString displayStr_splashImageAddedCount = "Images Added: ";
QString displayStr_splashImageRemovedCount = "Images Removed: ";
QString displayStr_splashImageTotalCount = "Total Images: ";
QStringList iniGUITokens;
bool FirstConnection=true;

extern INIPARAM_INFO iniParam_Info[NR_INI_TOKENS];

typedef enum _qListDataRoles
{
    TrigType,
    PatNum,
    BitDepth,
    LEDSelect,
    InvertPat,
    InsertBlack,
    BufSwap,
    trigOutPrev,
    frameIndex,
}qListDataRoles;

typedef enum _trigInType
{
    IntTrig,
    ExtPos,
    ExtNeg,
    NoTrig,
}trigInType;


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    settings("Texas Instruments", "LightCrafter GUI")
{
    ui->setupUi(this);

    // remove Peripheral Tab
    //ui->tabWidget->removeTab(2);

    // Hide CreateSplash Tab
    //ui->tabWidget->removeTab(2);

    // Hide the Connect button
    ui->ConnectPushButton->hide();

    // Hide the invert LED pwn checkbox
    ui->LedPwmInvert_checkBox->hide();

    // Hide Download Pattern button
    ui->SplashImageLoadButton->hide();

    // Hide Skip Bootloader checkbox
    ui->skipBL_checkBox->hide();

    // Hide Bitplane selection combobox
    ui->BitPlane_comboBox->hide();
    ui->BitPlane_label->hide();

    // Hide Import/Export CSV buttons
    ui->Export_pushButton->hide();
    ui->Import_pushButton->hide();

    //Hide Default Solution and choose .ini file buttons
//    ui->selectIniFileButton->hide();

    USB_Init();
    emit on_ConnectPushButton_clicked();

    firmwarePath = settings.value("FirmwarePath", "").toString();
    ptnImagePath = settings.value("PtnImagePath", "").toString();
    outFileName = settings.value("outFileName", "").toString();
    csvFileName = settings.value("csvFileName", "").toString();

    ui->frame_splash_index->setValue(0);
    updateFrameTitle();
    pImageBuffer = (unsigned char *)malloc(PTN_WIDTH*PTN_HEIGHT*BYTES_PER_PIXEL);
    if(pImageBuffer == NULL)
        exit(-1);
    memset(pImageBuffer, 0, PTN_WIDTH*PTN_HEIGHT*BYTES_PER_PIXEL);
    ui->outBmpFileName->setText(outFileName);
    emit on_patFrom_flash_toggled(true);

    /* Set up some of the line edit boxes to receive only numerical value inputs */
    ui->trigPeriod_lineEdit->setValidator(new QIntValidator(ui->trigPeriod_lineEdit));
    ui->patExp_lineEdit->setValidator(new QIntValidator(ui->patExp_lineEdit));
    ui->framePer_lineEdit->setValidator(new QIntValidator(ui->framePer_lineEdit));
    QIntValidator *inputRange = new QIntValidator(0, 255, this);
    ui->RedLEDCurrent->setValidator(inputRange);
    ui->GreenLEDCurrent->setValidator(inputRange);
    ui->BlueLEDCurrent->setValidator(inputRange);
    ui->PWM_Period_lineEdit->setValidator(new QIntValidator(ui->PWM_Period_lineEdit));
    ui->PWM_SampleRate_lineEdit->setValidator(new QIntValidator(ui->PWM_SampleRate_lineEdit));
    ui->trigPeriod_lineEdit->setText(numToStr(10000));
    ui->patExp_lineEdit->setText(numToStr(10000));

    iniGUITokens << "DEFAULT.DISPMODE" << "DEFAULT.SHORT_FLIP" << "DEFAULT.LONG_FLIP" << "DEFAULT.TRIG_OUT_1.POL" << "DEFAULT.TRIG_OUT_1.RDELAY" <<
	    "DEFAULT.TRIG_OUT_1.FDELAY" << "DEFAULT.TRIG_OUT_2.POL" << "DEFAULT.TRIG_OUT_2.WIDTH" << "DEFAULT.TRIG_IN_1.DELAY" << "DEFAULT.RED_STROBE.RDELAY" <<
	    "DEFAULT.RED_STROBE.FDELAY" << "DEFAULT.GRN_STROBE.RDELAY" << "DEFAULT.GRN_STROBE.FDELAY" << "DEFAULT.BLU_STROBE.RDELAY" << "DEFAULT.BLU_STROBE.FDELAY" <<
	    "DEFAULT.INVERTDATA" << "DEFAULT.LEDCURRENT_RED" << "DEFAULT.LEDCURRENT_GRN" << "DEFAULT.LEDCURRENT_BLU" << "DEFAULT.PATTERNCONFIG.PAT_EXPOSURE" <<
	    "DEFAULT.PATTERNCONFIG.PAT_PERIOD" << "DEFAULT.PATTERNCONFIG.PAT_MODE" << "DEFAULT.PATTERNCONFIG.TRIG_MODE" << "DEFAULT.PATTERNCONFIG.PAT_REPEAT"<<
	    "DEFAULT.SPLASHLUT" << "DEFAULT.SEQPATLUT" << "DEFAULT.PORTCONFIG.PORT" << "DEFAULT.PORTCONFIG.BPP" << "DEFAULT.PORTCONFIG.PIX_FMT" <<
	    "DEFAULT.PORTCONFIG.PORT_CLK" << "DEFAULT.PORTCONFIG.ABC_MUX" << "DEFAULT.PORTCONFIG.PIX_MODE" << "DEFAULT.PORTCONFIG.SWAP_POL" << "DEFAULT.PORTCONFIG.FLD_SEL";


    usbPollTimer = new QTimer(this);
    usbPollTimer->setInterval(2000);
    connect(usbPollTimer, SIGNAL(timeout()), this, SLOT(timerTimeout()));
    usbPollTimer->start();
}

MainWindow::~MainWindow()
{
    int i;

    USB_Close();
    USB_Exit();
    settings.setValue("FirmwarePath", firmwarePath);
    settings.setValue("PtnImagePath", ptnImagePath);
    settings.setValue("outFileName", outFileName);
    settings.setValue("csvFileName", csvFileName);
    free(pImageBuffer);
    for(i = 0; i < MAX_SPLASH_IMAGES; i++)
    {
	char file_name[11];

	sprintf(file_name, "temp_%d.bmp", i);
    	QFile outFile(file_name);

	outFile.remove();
    }
    
    delete ui;
}

static int My_FileRead(void *Param, unsigned char *Data, unsigned int Size)
{
    QDataStream *pIn = (QDataStream *)Param;

    if(Data == NULL)
    {
        return 0;
    }
    else if(Size > 0)
    {
        if(pIn->readRawData((char *)Data, Size) <= 0)
            return -1;
    }

    return 0;
}

static int My_ImgeDraw(void *Param, unsigned int X, unsigned int Y, unsigned char *Pix, unsigned int Count)
{
    unsigned char bitPos = *(unsigned char *)Param & 0x1F; // LSB 5 bits represent where to place the pixel in stitched output image
    unsigned char bitDepth = (*(unsigned char *)Param >> 5) + 1; //MSB 3-bits represent bit depth of input image (zero based)
    unsigned int i;
    unsigned char pixelByte, imgByte, bitMask;
    unsigned int bytePos=0;

    if(bitPos > 23)
        return -1;
    else if(bitPos >15)
        bytePos = 2;
    else if(bitPos > 7)
        bytePos=1;

    bitPos -= bytePos*8; //relative position in the output Byte;

    if(X >= PTN_WIDTH || Y >= PTN_HEIGHT)
            return 0;

    if(X + Count > PTN_WIDTH)
    {
        Count = PTN_WIDTH - X;
    }

    for(i=0; i<Count; i++)
    {
        imgByte = *(pImageBuffer + (X + i + Y * PTN_WIDTH)*BYTES_PER_PIXEL + bytePos);
        pixelByte = *(Pix+i);
        pixelByte <<= bitPos;
        bitMask = POW_OF_2(bitDepth)-1;
        bitMask <<= bitPos;
        imgByte &= ~bitMask;
        imgByte |= pixelByte;
        *(pImageBuffer + (X + i + Y * PTN_WIDTH)*BYTES_PER_PIXEL + bytePos) = imgByte;

        if(bitPos+bitDepth > 8)
        {
            imgByte = *(pImageBuffer + (X + i + Y * PTN_WIDTH)*BYTES_PER_PIXEL + bytePos + 1);
            pixelByte = *(Pix+i);
            pixelByte >>= (8-bitPos);
            bitMask = POW_OF_2(8-bitPos)-1;
            imgByte &= ~bitMask;
            imgByte |= pixelByte;
            *(pImageBuffer + (X + i + Y * PTN_WIDTH)*BYTES_PER_PIXEL + bytePos + 1) = imgByte;
        }
    }
    //memcpy(pImageBuffer+(X + Y * PTN_WIDTH)*BYTES_PER_PIXEL, Pix, Count);

    return 0;
}

static int My_FileWrite(void *Param, unsigned char *Data, unsigned int Size)
{
    QFile *pOutFile = (QFile *)Param;

    if(Data == NULL)
    {
        if(pOutFile->seek(Size) != true)
            return -1;
    }
    else if(Size > 0)
    {
        if(pOutFile->write((char *)Data, Size) <= 0)
            return -1;
    }

    return 0;
}

static int My_ImgeGet(void *Param, unsigned int X, unsigned int Y, unsigned char *Pix, unsigned int Count)
{
    if(X >= PTN_WIDTH || Y >= PTN_HEIGHT)
        return 0;
    if(X + Count > PTN_WIDTH)
    {
        Count = PTN_WIDTH - X;
    }
    memcpy(Pix, pImageBuffer + (X + Y * PTN_WIDTH) * BYTES_PER_PIXEL, Count * BYTES_PER_PIXEL);
    return 0;
}
void MainWindow::timerTimeout(void)
{
    if(USB_IsConnected())
    {
        if(on_GetStatusPushButton_clicked() != 0)
        {
            emit on_ConnectPushButton_clicked();
        }
    }
    else
    {
        emit on_ConnectPushButton_clicked();
    }
}

void MainWindow::showError(const char *str)
{
    QString title("LightCrafter Error Message");
    QString text(str);
    QMessageBox msgBox(QMessageBox::Warning, title, text, QMessageBox::NoButton, this);
    msgBox.exec();
}

void MainWindow::on_ConnectPushButton_clicked()
{
    char versionStr[255];
    //unsigned int API_ver, App_ver, SWConfig_ver, SeqConfig_ver;
    unsigned int FW_ver;
    bool SLmode=0;

    if(USB_IsConnected())
        USB_Close();
    USB_Open();
    ui->ConnectToolButton->setEnabled(USB_IsConnected());

    // Display GUI Version #
    sprintf(versionStr, "DLP LightCrafter 4500 Control Software - %d.%d.%d", GUI_VERSION_MAJOR, GUI_VERSION_MINOR, GUI_VERSION_BUILD);
    setWindowTitle(versionStr);

    if(USB_IsConnected())
    {
        /*
        if (LCR_GetVersion(&App_ver, &API_ver, &SWConfig_ver, &SeqConfig_ver) == 0)
        {
            sprintf(versionStr, "%d.%d.%d", (API_ver >> 24), ((API_ver << 8) >> 24), ((API_ver << 16) >> 16));
            ui->API_version->setText(versionStr);
            sprintf(versionStr, "%d.%d.%d", (App_ver >> 24), ((App_ver << 8) >> 24), ((App_ver << 16) >> 16));
            ui->App_Version->setText(versionStr);
            sprintf(versionStr, "%d.%d.%d", (SWConfig_ver >> 24), ((SWConfig_ver << 8) >> 24), ((SWConfig_ver << 16) >> 16));
            ui->SWConfig_version->setText(versionStr);
            sprintf(versionStr, "%d.%d.%d", (SeqConfig_ver >> 24), ((SeqConfig_ver << 8) >> 24), ((SeqConfig_ver << 16) >> 16));
            ui->SeqConfig_Version->setText(versionStr);
        }
        */
        if(LCR_MemRead(0xF902C000, &FW_ver) == 0)
        {
            FW_ver &= 0xFFFFFF;
            sprintf(versionStr, "%d.%d.%d", (FW_ver >> 16), ((FW_ver << 16) >> 24), ((FW_ver << 24) >> 24));
            ui->API_version->setText(versionStr);

            // When GUI is first opened, check if old firmware is present & prompt user to upload new version if it is
            if (FirstConnection && (FW_ver < RELEASE_FW_VERSION))
            {
                showError("WARNING: Old version of Firmware detected.\n\nDownload the latest release from http://www.ti.com/tool/dlpr350.");
                FirstConnection = FALSE;
             }
        }

        emit on_GetStatusPushButton_clicked();

        if(LCR_GetMode(&SLmode) == 0)
        {
            if(SLmode)
            {
                ui->Radiobutton_SLmode->setChecked(true);
                ui->RadioButton_videoMode->setChecked(false);
            }
            else
            {
                ui->Radiobutton_SLmode->setChecked(false);
                ui->RadioButton_videoMode->setChecked(true);
            }
        }
    }
}

void MainWindow::on_GetLEDpushButton_clicked()
{
    bool SeqCtrl, Red, Green, Blue, inverted;
    unsigned char RedCurrent, GreenCurrent, BlueCurrent;
    char currentStr[8];

    if (LCR_GetLedEnables(&SeqCtrl, &Red, &Green, &Blue) == 0)
    {
        ui->radioButton_ColorDisplayAuto->setChecked(SeqCtrl);
        ui->radioButton_ColorDisplayManual->setChecked(!SeqCtrl);

        if(SeqCtrl)
        {
            ui->RedCheckBox->setDisabled(true);
            ui->GreenCheckBox->setDisabled(true);
            ui->BlueCheckBox->setDisabled(true);
        }
        else
        {
            ui->RedCheckBox->setChecked(Red);
            ui->GreenCheckBox->setChecked(Green);
            ui->BlueCheckBox->setChecked(Blue);
        }
    }


    if (LCR_GetLedCurrents(&RedCurrent, &GreenCurrent, &BlueCurrent) == 0)
    {
        sprintf(currentStr,"%d", 255-RedCurrent);
        ui->RedLEDCurrent->setText(currentStr);
        sprintf(currentStr,"%d", 255-GreenCurrent);
        ui->GreenLEDCurrent->setText(currentStr);
        sprintf(currentStr,"%d", 255-BlueCurrent);
        ui->BlueLEDCurrent->setText(currentStr);
    }

    if(LCR_GetLEDPWMInvert(&inverted) == 0)
    {
        ui->LedPwmInvert_checkBox->setChecked(inverted);
    }
}

void MainWindow::on_SetLEDpushButton_clicked()
{
    bool SeqCtrl, Red, Green, Blue;
    unsigned char RedCurrent, GreenCurrent, BlueCurrent;

    // Should the LEDs be controlled by the sequence?
    SeqCtrl = ui->radioButton_ColorDisplayAuto->isChecked();
    Red = ui->RedCheckBox->isChecked();
    Green = ui->GreenCheckBox->isChecked();
    Blue = ui->BlueCheckBox->isChecked();

    LCR_SetLedEnables(SeqCtrl, Red, Green, Blue);

    LCR_SetLEDPWMInvert(ui->LedPwmInvert_checkBox->isChecked());

    RedCurrent      = 255-strToNum(ui->RedLEDCurrent->text());
    GreenCurrent    = 255-strToNum(ui->GreenLEDCurrent->text());
    BlueCurrent     = 255-strToNum(ui->BlueLEDCurrent->text());

    LCR_SetLedCurrents(RedCurrent, GreenCurrent, BlueCurrent);
}



void MainWindow::on_radioButton_ColorDisplayAuto_clicked()
{
    //Disable the manual color display checkboxes
    ui->RedCheckBox->setDisabled(true);
    ui->GreenCheckBox->setDisabled(true);
    ui->BlueCheckBox->setDisabled(true);
}

void MainWindow::on_radioButton_ColorDisplayManual_clicked()
{
    //Enable the manual color display checkboxes
    ui->RedCheckBox->setDisabled(false);
    ui->GreenCheckBox->setDisabled(false);
    ui->BlueCheckBox->setDisabled(false);
}


void MainWindow::on_GetFlipPushButton_clicked()
{
    ui->LongAxisFlipCheckBox->setChecked(LCR_GetLongAxisImageFlip());
    ui->ShortAxisFlipCheckBox->setChecked(LCR_GetShortAxisImageFlip());
}

void MainWindow::on_SetFlipPushButton_clicked()
{
    LCR_SetLongAxisImageFlip(ui->LongAxisFlipCheckBox->isChecked());
    LCR_SetShortAxisImageFlip(ui->ShortAxisFlipCheckBox->isChecked());
}

void MainWindow::on_FirmwareBrowseBut_2_clicked()
{
    QString fileName;

    fileName = QFileDialog::getOpenFileName(this,
                                QString("Select Image to load"),
                                            firmwarePath,
                                "*.img *.bin");

    ui->FirmwareLoadBut->setEnabled(false);

    if(!fileName.isEmpty())
    {
	QFile imgFileIn(fileName);

        ui->FirmwareFile_2->setText(fileName);
        QFileInfo firmwareFileInfo;
        firmwareFileInfo.setFile(fileName);
        firmwarePath = firmwareFileInfo.absolutePath();
    }
    ui->FirmwareLoadBut->setEnabled(true);    
}

void MainWindow::on_FirmwareBrowseBut_clicked()
{
    QString fileName;
    int count;

    fileName = QFileDialog::getOpenFileName(this,
                                QString("Select Image to load"),
                                            firmwarePath,
                                "*.img *.bin");

    count = ui->splashImage_comboBox->count();
    ui->splashImage_comboBox->setEnabled(false);
    ui->addSplashImageButton->setEnabled(false);
    ui->removeSplashImagepushButton->setEnabled(false);
    ui->changeSplashImagepushButton->setEnabled(false);
    ui->buildNewFrmwImageButton->setEnabled(false);
    ui->selectIniFileButton->setEnabled(false);
//    ui->SplashImageLoadButton->setEnabled(true);
    ui->newFirmwareFile->clear();
    ui->newFirmwareFile->setEnabled(false);
    ui->dispSplashImage->clear();
    ui->splashImageCount->setText(displayStr_splashImageCount);
    ui->splashImageAddedCount->setText(displayStr_splashImageAddedCount + QString::number(0));
    ui->splashImageRemovedCount->setText(displayStr_splashImageRemovedCount + QString::number(0));
    ui->splashImageTotalCount->setText(displayStr_splashImageTotalCount);
    ui->Dnld_progressBar->setValue(0);
    for (int i = 0; i < count - 1; i++)
	ui->splashImage_comboBox->removeItem(1);

    splashImageAddIndex = 0;
    splashImageCount = 0;
    splashImageAdded = 0;
    splashImageRemoved = 0;
    if(!fileName.isEmpty())
    {
	QFile imgFileIn(fileName);
	int splash_count, actual_splash_count, ret;
	char displayStr[255];
        unsigned char *pByteArray;
        int fileLen;
	int i;
        ui->FirmwareFile->setText(fileName);
        QFileInfo firmwareFileInfo;
        firmwareFileInfo.setFile(fileName);
        firmwarePath = firmwareFileInfo.absolutePath();
	if(!imgFileIn.open(QIODevice::ReadWrite))
	{
		showError("Unable to open image file. Copy image file to a folder with Admin/read/write permission and try again\n");
		return;
	}
       for (i = 0; i < MAX_SPLASH_IMAGES; i++)
	       added_SplashImages[i].clear();

       fileLen = imgFileIn.size();
       pByteArray = (unsigned char *)malloc(fileLen);

       if(pByteArray == NULL)
       {
           imgFileIn.close();
           showError("Memory alloc for file read failed");
           return;
       }
       imgFileIn.read((char *)pByteArray, fileLen);
       imgFileIn.close();
       
       ret = Frmw_CopyAndVerifyImage(pByteArray, fileLen);
       if (ret)
       {
	       switch(ret)
	       {
		       case ERROR_FRMW_FLASH_TABLE_SIGN_MISMATCH:
	                showError("ERROR: Flash Table Signature doesn't match! Bad Firmware Image!\n");
        	        free(pByteArray);
                	return;
		       case ERROR_NO_MEM_FOR_MALLOC:
			showError("Fatal Error! System Run out of memory\n");
			free(pByteArray);
			return;
		       default:
			break;
            }
       }

       free(pByteArray);

       if ((Frmw_GetVersionNumber() & 0xFFFFFF) < RELEASE_FW_VERSION)
		showError("WARNING: Old version of Firmware detected.\n\nDownload the latest release from http://www.ti.com/tool/dlpr350.");

       splash_count = Frmw_GetSplashCount();
       actual_splash_count = splash_count;
       if (splash_count < 0)
       {
        showError("Firmware file image header format not recognized\n");
       }
       else
       {
	    ui->Progressbar_action_label->setText("Retrieving Pattern Images\n");
	    for (i = 0; i < splash_count; i++)
	    {
		char file_name[11];
			
		QApplication::processEvents(); //Update GUI

		ui->Dnld_progressBar->setValue(i*100/(splash_count - 1));

	        sprintf(file_name, "temp_%d.bmp", i);
    		QFile outFile(file_name);

	        if(outFile.open(QIODevice::ReadWrite))
		{
		    BMP_Image_t splashImage;

	            ret = Frmw_GetSpashImage(pImageBuffer, i);

		    if (ret)
		    {
		            outFile.close();
			    memset(pImageBuffer, 0, PTN_WIDTH*PTN_HEIGHT*BYTES_PER_PIXEL);
			    switch(ret)
			    {
				    case ERROR_NO_MEM_FOR_MALLOC:
					showError("Fatal Error! System Run out of memory\n");
					return;
				    case ERROR_NO_SPLASH_IMAGE:
					actual_splash_count--;
					continue;
				    default:
					continue;
			    }
		    }
    		    BMP_InitImage(&splashImage, PTN_WIDTH, PTN_HEIGHT, 8*BYTES_PER_PIXEL);
		    BMP_StoreImage(&splashImage, (BMP_DataFunc_t *)My_FileWrite, &outFile, (BMP_PixelFunc_t *)My_ImgeGet, NULL);
	            outFile.close();
		    memset(pImageBuffer, 0, PTN_WIDTH*PTN_HEIGHT*BYTES_PER_PIXEL);
		    added_SplashImages[i] = outFile.fileName();
		    if (i)
		            ui->splashImage_comboBox->addItem(QString::number(i));		    
		}
    		else
		        showError("Cannot save pattern image as .bmp to display. Try relocating GUI to a folder that has write permission");
	    }

	    splashImageAddIndex = actual_splash_count;
	    splashImageCount = actual_splash_count;
	    
	    sprintf(displayStr, "%d", actual_splash_count);
	    ui->splashImageCount->setText(displayStr_splashImageCount + displayStr);
            ui->splashImageTotalCount->setText(displayStr_splashImageTotalCount + displayStr);
            ui->splashImage_comboBox->setEnabled(true);
	    ui->addSplashImageButton->setEnabled(true);
	    ui->removeSplashImagepushButton->setEnabled(true);
	    ui->changeSplashImagepushButton->setEnabled(true);    
	    ui->buildNewFrmwImageButton->setEnabled(true);
	    ui->selectIniFileButton->setEnabled(true);
//	    ui->SplashImageLoadButton->setEnabled(true);
	    ui->newFirmwareFile->setEnabled(true);
            ui->splashImage_comboBox->setCurrentIndex(0);
            emit on_splashImage_comboBox_currentIndexChanged(0);
	    ui->Progressbar_action_label->setText("Retrieved Pattern Images\n");
       }
    }
}

int MainWindow::GetSectorNum(unsigned int Addr)
{
    unsigned int i;
    for(i=0; i < myFlashDevice.numSectors; i++)
    {
        if(myFlashDevice.SectorArr[i] > Addr)
            break;
    }
    return i-1;
}

bool MainWindow::process_flash_params_line(QString line)
{
    unsigned int MfgID, DevID, i;
    bool ok;

    line = line.trimmed();
    if(line.startsWith('/'))
        return false;

    if(line.isEmpty())
        return false;

    MfgID = line.section(',',1,1).trimmed().toUInt(&ok, 0);
    if(!ok)
        return false;
    DevID = line.section(',',3,3).trimmed().toUInt(&ok, 0);
    if(!ok)
        return false;

    if((MfgID == myFlashDevice.Mfg_ID) && (DevID == myFlashDevice.Dev_ID))
    {
        myFlashDevice.Mfg = line.section(',', 0, 0).trimmed();
        myFlashDevice.Dev = line.section(',', 2, 2).trimmed();
        myFlashDevice.Size_MBit = line.section(',', 4, 4).trimmed().toUInt(&ok, 0);
        if(!ok)
            return false;
        myFlashDevice.Type = line.section(',', 5, 5).trimmed().toUInt(&ok, 0);
        if(!ok)
            return false;
        myFlashDevice.numSectors = line.section(',', 7, 7).trimmed().toUInt(&ok, 0);
        if(!ok)
            return false;

        for(i=0; i<myFlashDevice.numSectors; i++)
        {
            myFlashDevice.SectorArr[i] = line.section(',', 8+i, 8+i).trimmed().toUInt(&ok, 0);
            if(!ok)
                return false;
        }

        return true;
    }
    return false;
}

void MainWindow::on_FirmwareLoadBut_clicked()
{
    QFile imgFile(ui->FirmwareFile_2->text());
    QFile flashParamFile(":/new/prefix1/Flash/FlashDeviceParameters.txt");
    unsigned short manID;
    unsigned long long devID;
    char displayStr[255];
    int startSector=0, i, BLsize=0, lastSectorToErase;
    unsigned char *pByteArray=NULL;
    long long dataLen, dataLen_full;
    int bytesSent;
    unsigned int expectedChecksum=0, checksum,FW_Flash_ver, FW_Image_ver ;
    long long percent_completion = 0;

    if(!imgFile.open(QIODevice::ReadOnly))
    {
        showError("Unable to open image file. Copy image file to a folder with Admin/read/write permission and try again\n");
        return;
    }

    if(LCR_MemRead(0xF902C000, &FW_Flash_ver) == 0)
    {
	    FW_Flash_ver &= 0xFFFFFF;
        FW_Image_ver = Frmw_GetVersionNumber();
	    FW_Image_ver &= 0xFFFFFF;

	    if ((FW_Image_ver >= RELEASE_FW_VERSION) && (FW_Flash_ver < RELEASE_FW_VERSION))
	    {
		/* Disabling User Prompting to download Bootloader. Enable it back if needed */
/*		QString text("New Bootloader available in the chosen image. Do you want to upgrade your board to the new BootLoader");
		QMessageBox msgBox(QMessageBox::Warning, "LightCrafter Message", text, QMessageBox::Yes|QMessageBox::No, this);
		if(msgBox.exec() == QMessageBox::Yes)*/
			ui->skipBL_checkBox->setChecked(false);		    
	    }
    }
    if(LCR_EnterProgrammingMode() < 0)
    {
        showError("Unable to enter Programming mode");
        return;
    }

    ui->Progressbar_action_label_2->setText("Waiting to enter programming mode");

    QTime waitEndTime = QTime::currentTime().addSecs(5);
    //If going from normal mode to bootloader mode, wait until connection closes; then wait again for reconnection
    while(ui->ConnectToolButton->isEnabled() == true)
    {
        QApplication::processEvents(); //Update the GUI
        //timeout in case the target is already in bootloader mode.
        if(QTime::currentTime() > waitEndTime)
            break;
    }

    while(ui->ConnectToolButton->isEnabled() == false)
    {
        QApplication::processEvents(); //Update the GUI
    }

    usbPollTimer->stop();

    if(LCR_GetFlashManID(&manID) < 0)
    {
        showError("Unable to read Flash Manufacturer ID");
        return;
    }
    if(LCR_GetFlashDevID(&devID) < 0)
    {
        showError("Unable to read Flash Device ID");
        return;
    }
    devID &= 0xFFFF;

    myFlashDevice.Mfg_ID = manID;
    myFlashDevice.Dev_ID = devID;

    if (!flashParamFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        showError("Unable to open FlashDeviceParameters.txt");
        return;
    }

    QTextStream in(&flashParamFile);
    bool found = false;
    while (!in.atEnd())
    {
        QString line = in.readLine();
        if(process_flash_params_line(line))
        {
            found = true;
            break;
        }
    }

    if(found == false)
    {
        sprintf(displayStr, "Unsupported Flash Device : Manufacturer ID = 0x%x & Device ID = 0x%llx", manID, devID);
        showError(displayStr);
        return;
    }

    //LCR_BLSpecialMode(BIT0); //Skip Software Write Response from Bootloader

    if(ui->skipBL_checkBox->isChecked())
    {
        BLsize = 128 * 1024;
    }

    startSector = GetSectorNum(BLsize);
    lastSectorToErase = GetSectorNum(imgFile.size());
    if(imgFile.size() == myFlashDevice.SectorArr[lastSectorToErase]) //If perfectly aligned with last sector start addr, no need to erase last sector.
        lastSectorToErase -= 1;

    LCR_SetFlashType(myFlashDevice.Type);
    ui->Dnld_progressBar_2->setValue(0);
    ui->Progressbar_action_label_2->setText("Erasing Flash Sectors");

    for(i=startSector; i <= lastSectorToErase; i++)
    {
        LCR_SetFlashAddr(myFlashDevice.SectorArr[i]);
        LCR_FlashSectorErase();
        LCR_WaitForFlashReady();    //Wait for flash busy flag to go off
        ui->Dnld_progressBar_2->setValue(i*100/lastSectorToErase);
        QApplication::processEvents(); //Update the GUI
    }

    ui->Progressbar_action_label_2->setText("Erasing Flash Sectors Complete");
    dataLen = imgFile.size();
    pByteArray = (unsigned char *)malloc(dataLen);
    if(pByteArray == NULL)
    {
        imgFile.close();
        showError("Memory alloc for file read failed");
        return;
    }
    imgFile.read((char *)pByteArray, dataLen);

    dataLen -= BLsize;

    LCR_SetFlashAddr(BLsize);
    LCR_SetDownloadSize(dataLen);

    dataLen_full = dataLen;
    ui->Dnld_progressBar_2->setValue(0);
    ui->Progressbar_action_label_2->setText("Downloading Firmware Image");

    while(dataLen > 0)
    {
        bytesSent = LCR_DownloadData(pByteArray+BLsize+dataLen_full-dataLen, dataLen);

        if(bytesSent < 0)
        {
            showError("Flash Data Download Failed");
            imgFile.close();
            free(pByteArray);
            return;
        }
        for(i=0; i<bytesSent; i++)
        {
            expectedChecksum += pByteArray[BLsize+dataLen_full-dataLen+i];
        }

        dataLen -= bytesSent;
        if(percent_completion != (((dataLen_full-dataLen)*100)/dataLen_full))
        {
            percent_completion = (((dataLen_full-dataLen)*100)/dataLen_full);
            ui->Dnld_progressBar_2->setValue(percent_completion);
        }
        QApplication::processEvents(); //Update the GUI
    }
    ui->Progressbar_action_label_2->setText("Waiting for checksum verification");
    QApplication::processEvents(); //Update the GUI

    LCR_CalculateFlashChecksum();

#if 0
    /* More wait time for bigger files */
    if(imgFile.size() > 16*1024*1024)
        waitEndTime = QTime::currentTime().addSecs(6);
    else
        waitEndTime = QTime::currentTime().addSecs(3);

    while(true)
    {
        QApplication::processEvents(); //Update the GUI
        if(QTime::currentTime() > waitEndTime)
            break;
    }
#endif

    LCR_WaitForFlashReady();

    if(LCR_GetFlashChecksum(&checksum) < 0)
    {
        showError("Error reading checksum from target");
    }
    else  if(checksum != expectedChecksum)
    {
        sprintf(displayStr, "Checksum mismatch: Expected %x; Received %x", expectedChecksum, checksum);
        showError(displayStr);
    }
    else
    {
        LCR_ExitProgrammingMode(); //Exit programming mode; Start application.
        showError("Download Complete");
    }

    ui->skipBL_checkBox->setChecked(true);    
    ui->Progressbar_action_label_2->setText("Download Complete");
    ui->Dnld_progressBar_2->setValue(0);
    imgFile.close();
    free(pByteArray);
    usbPollTimer->start();

}

void MainWindow::on_Red_R_spinBox_valueChanged(int arg1)
{
    char displayStr[255];
    float val;
    ui->Red_R_Slider->setValue(arg1);

    val = (arg1-187)*107.2;

    sprintf(displayStr, "%2.2f us", val/1000);
    ui->Red_R_val->setText(displayStr);
}

void MainWindow::on_Red_R_Slider_valueChanged(int value)
{
    ui->Red_R_spinBox->setValue(value);
}

void MainWindow::on_Red_F_spinBox_valueChanged(int arg1)
{
    char displayStr[255];
    float val;
    ui->Red_F_Slider->setValue(arg1);

    val = (arg1-187)*107.2;

    sprintf(displayStr, "%2.2f us", val/1000);
    ui->Red_F_val->setText(displayStr);
}

void MainWindow::on_Red_F_Slider_valueChanged(int value)
{
    ui->Red_F_spinBox->setValue(value);
}

void MainWindow::on_Green_R_spinBox_valueChanged(int arg1)
{
    char displayStr[255];
    float val;
    ui->Green_R_Slider->setValue(arg1);

    val = (arg1-187)*107.2;

    sprintf(displayStr, "%2.2f us", val/1000);
    ui->Green_R_val->setText(displayStr);
}

void MainWindow::on_Green_R_Slider_valueChanged(int value)
{
    ui->Green_R_spinBox->setValue(value);
}

void MainWindow::on_Green_F_spinBox_valueChanged(int arg1)
{
    char displayStr[255];
    float val;
    ui->Green_F_Slider->setValue(arg1);

    val = (arg1-187)*107.2;

    sprintf(displayStr, "%2.2f us", val/1000);
    ui->Green_F_val->setText(displayStr);
}

void MainWindow::on_Green_F_Slider_valueChanged(int value)
{
    ui->Green_F_spinBox->setValue(value);
}

void MainWindow::on_Blue_R_spinBox_valueChanged(int arg1)
{
    char displayStr[255];
    float val;
    ui->Blue_R_Slider->setValue(arg1);

    val = (arg1-187)*107.2;

    sprintf(displayStr, "%2.2f us", val/1000);
    ui->Blue_R_val->setText(displayStr);
}

void MainWindow::on_Blue_R_Slider_valueChanged(int value)
{
    ui->Blue_R_spinBox->setValue(value);
}

void MainWindow::on_Blue_F_spinBox_valueChanged(int arg1)
{
    char displayStr[255];
    float val;
    ui->Blue_F_Slider->setValue(arg1);

    val = (arg1-187)*107.2;

    sprintf(displayStr, "%2.2f us", val/1000);
    ui->Blue_F_val->setText(displayStr);
}

void MainWindow::on_Blue_F_Slider_valueChanged(int value)
{
    ui->Blue_F_spinBox->setValue(value);
}

int MainWindow::on_GetStatusPushButton_clicked()
{
    unsigned char HWStatus, SysStatus, MainStatus;

    if(LCR_GetStatus(&HWStatus, &SysStatus, &MainStatus) == 0)
    {
        ui->IndicatorButton_1->setEnabled((HWStatus & BIT0) == BIT0); //Init Done
        ui->IndicatorButton_2->setEnabled((HWStatus & BIT3) == BIT3); //Forced Swap
        ui->IndicatorButton_3->setEnabled((HWStatus & BIT6) == BIT6); //Sequence Abort
        ui->IndicatorButton_4->setEnabled((HWStatus & BIT2) == BIT2); // DRC Error
        ui->IndicatorButton_5->setEnabled((HWStatus & BIT7) == BIT7); // Sequence Error


        ui->IndicatorButton_8->setEnabled((MainStatus & BIT0) == BIT0); //DMD Parked
        ui->IndicatorButton_9->setEnabled((MainStatus & BIT1) == BIT1); // Sequence Running
        ui->IndicatorButton_10->setEnabled((MainStatus & BIT2) == BIT2); //Buffer frozen
         return 0;
    }
    else
    {
        ui->IndicatorButton_1->setEnabled(false);
        ui->IndicatorButton_2->setEnabled(false);
        ui->IndicatorButton_3->setEnabled(false);
        ui->IndicatorButton_4->setEnabled(false);
        ui->IndicatorButton_5->setEnabled(false);
        ui->IndicatorButton_8->setEnabled(false);
        ui->IndicatorButton_9->setEnabled(false);
        ui->IndicatorButton_10->setEnabled(false);
        return -1;
    }
}

void MainWindow::on_pushButton_Reset_clicked()
{
    LCR_SoftwareReset();
}



void MainWindow::on_GetLEDpushButton_2_clicked()
{
    unsigned char rising, falling;

    if(LCR_GetRedLEDStrobeDelay(&rising, &falling) == 0)
    {
        ui->Red_R_spinBox->setValue(rising);
        ui->Red_F_spinBox->setValue(falling);
    }
    if(LCR_GetGreenLEDStrobeDelay(&rising, &falling) == 0)
    {
        ui->Green_R_spinBox->setValue(rising);
        ui->Green_F_spinBox->setValue(falling);
    }
    if(LCR_GetBlueLEDStrobeDelay(&rising, &falling) == 0)
    {
        ui->Blue_R_spinBox->setValue(rising);
        ui->Blue_F_spinBox->setValue(falling);
    }

}

void MainWindow::on_SetLEDpushButton_2_clicked()
{
    LCR_SetRedLEDStrobeDelay(ui->Red_R_spinBox->value(), ui->Red_F_spinBox->value());
    LCR_SetGreenLEDStrobeDelay(ui->Green_R_spinBox->value(), ui->Green_F_spinBox->value());
    LCR_SetBlueLEDStrobeDelay(ui->Blue_R_spinBox->value(), ui->Blue_F_spinBox->value());
}

void MainWindow::on_SourceSetBut_clicked()
{
    unsigned int index = ui->InputSourceList->currentIndex();
    if(index == 0 || index == 3)
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

void MainWindow::on_SourceGetBut_clicked()
{
    unsigned int source, portWidth, pattern, splashindex;

    if(LCR_GetInputSource(&source, &portWidth) >= 0)
    {
        ui->InputSourceList->setCurrentIndex(source);
        if(source == 0 || source == 3)
            ui->InputSourceOptionList->setCurrentIndex(portWidth);
        else if(source == 1)
        {
            LCR_GetTPGSelect(&pattern);
            ui->InputSourceOptionList->setCurrentIndex(pattern);
        }
        else
        {
            LCR_GetSplashIndex(&splashindex);
            ui->InputSourceOptionList->setCurrentIndex(splashindex);
        }
    }
}

void MainWindow::on_InputSourceList_currentIndexChanged(int index)
{
    QStringList PixelFormats;
    QStringList PortWidth  = QStringList() << "30 BITS" << "24 BITS" << "20 BITS" << "16 BITS" << "10 BITS" << "8 BITS";
    QStringList TestPatterns = QStringList() << "Solid Field" << "Horizontal Ramp" << "Vertical Ramp" << "Horizontal Lines" << \
                                                "Diagonal Lines" << "Vertical Lines" << "Grid" << "Checkerboard" << "RGB Ramp" <<\
                                                "Color Bar" << "Step Bar";
    QStringList SplashIndices = QStringList() << "0" << "1" << "2" << "3" << "4" << "5" << "6" << "7" << "8" << "9" << \
                                                 "10" << "11" << "12" << "13" << "14" << "15" << "16" << "17" << "18" << "19" <<\
                                                 "20" << "21" << "22" << "23" << "24" << "25" << "26" << "27" << "28" << "29" <<
                                                 "30" << "31" << "32" << "33" << "34" << "35" << "36" << "37" << "38" << "39" <<\
                                                 "40" << "41" << "42" << "43" << "44" << "45" << "46" << "47" << "48" << "49" <<\
                                                 "50" << "51" << "52" << "53" << "54" << "55" << "56" << "57" << "58" << "59" <<\
                                                 "60" << "61" << "62" << "63";

    ui->TPGColorSetBut->setEnabled(false);
    ui->TPGColorGetBut->setEnabled(false);
    ui->TPGRed_BG_spinBox->setEnabled(false);
    ui->TPGRed_FG_spinBox->setEnabled(false);
    ui->TPGBlue_BG_spinBox->setEnabled(false);
    ui->TPGBlue_FG_spinBox->setEnabled(false);
    ui->TPGGreen_BG_spinBox->setEnabled(false);
    ui->TPGGreen_FG_spinbox->setEnabled(false);

    ui->FPDFieldSelectList->setEnabled(false);
    ui->FPD_Inv_Pol_checkBox->setEnabled(false);
    ui->FPD_PixMode_spinBox->setEnabled(false);
    ui->FPDModeGetBut->setEnabled(false);
    ui->FPDModeSetBut->setEnabled(false);

    ui->SwapSelectList->setEnabled(false);
    ui->GetSwapBut->setEnabled(false);
    ui->SetSwapBut->setEnabled(false);

    ui->PortClockList->setEnabled(false);
    ui->PortClockGetBut->setEnabled(false);
    ui->PortClockSetBut->setEnabled(false);

    ui->InputSourceOptionList->clear();
    switch(index)
    {
    case 0:
        PixelFormats = QStringList() << "RGB 444" << "YUV 444" << "YUV 422";
        ui->InputSourceOptionList->addItems(PortWidth);
        ui->SwapSelectList->setEnabled(true);
        ui->GetSwapBut->setEnabled(true);
        ui->SetSwapBut->setEnabled(true);
        ui->PortClockList->setEnabled(true);
        ui->PortClockGetBut->setEnabled(true);
        ui->PortClockSetBut->setEnabled(true);
        break;
    case 1:
        PixelFormats = QStringList() << "RGB 444" ;
        ui->InputSourceOptionList->addItems(TestPatterns);
        ui->TPGColorSetBut->setEnabled(true);
        ui->TPGColorGetBut->setEnabled(true);
        ui->TPGRed_BG_spinBox->setEnabled(true);
        ui->TPGRed_FG_spinBox->setEnabled(true);
        ui->TPGBlue_BG_spinBox->setEnabled(true);
        ui->TPGBlue_FG_spinBox->setEnabled(true);
        ui->TPGGreen_BG_spinBox->setEnabled(true);
        ui->TPGGreen_FG_spinbox->setEnabled(true);
        break;
    case 2:
        PixelFormats = QStringList() << "RGB 444" << "YUV 422";
        ui->InputSourceOptionList->addItems(SplashIndices);
        break;
    case 3:
        PixelFormats = QStringList() << "RGB 444" ;
        ui->InputSourceOptionList->addItems(PortWidth);
        ui->FPDFieldSelectList->setEnabled(true);
        ui->FPD_Inv_Pol_checkBox->setEnabled(true);
        ui->FPD_PixMode_spinBox->setEnabled(true);
        ui->FPDModeGetBut->setEnabled(true);
        ui->FPDModeSetBut->setEnabled(true);

        ui->SwapSelectList->setEnabled(true);
        ui->GetSwapBut->setEnabled(true);
        ui->SetSwapBut->setEnabled(true);
        break;
    }
    ui->PixelFormatList->clear();
    ui->PixelFormatList->addItems(PixelFormats);
}

/*
void MainWindow::on_Radiobutton_SLmode_toggled(bool checked)
{
    LCR_SetMode(checked);
}*/

void MainWindow::on_RadioButton_videoMode_clicked()
{
    LCR_SetPowerMode(0);
    ui->Radiobutton_SLmode->setEnabled(true);
   // SleeperThread::msleep(50);
    LCR_SetMode(0);

    // Select the video mode tab
    //ui->tabWidget->setEnabled(true);
    ui->tabWidget->setCurrentIndex(0);
}

void MainWindow::on_Radiobutton_SLmode_clicked()
{
    //LCR_SetPowerMode(0);
    LCR_SetMode(1);

    // Select the video mode tab
    //ui->tabWidget->setEnabled(true);
    ui->tabWidget->setCurrentIndex(1);
}


void MainWindow::on_RadioButton_standbyMode_clicked()
{

    /* Go to video mode first before going power standby to avoid artifacts on screen */
    LCR_SetMode(0);
    /* Set power mode standby */
    LCR_SetPowerMode(1);
    ui->Radiobutton_SLmode->setEnabled(false);

    //Disable sending information
    //ui->tabWidget->setEnabled(false);
}


void MainWindow::on_PixelFormatSetBut_clicked()
{
    unsigned int PixelFormat = ui->PixelFormatList->currentIndex();

    //If input source selected is splash
    if(ui->InputSourceList->currentIndex() == 2)
    {
        //If second item in the pixel format box is selected, the value to be passed is 2 for YUV444.
        if(ui->PixelFormatList->currentIndex() == 1)
            PixelFormat = 2;
    }
    LCR_SetPixelFormat(PixelFormat);
}

void MainWindow::on_PixelFormatGetBut_clicked()
{
    unsigned int pixelFormat;

    //Get input source first so that the options for formatlist is correctly popoulated.
    emit on_SourceGetBut_clicked();

    if(LCR_GetPixelFormat(&pixelFormat) == 0)
    {
        //If input source selected is splash
        if(ui->InputSourceList->currentIndex() == 2)
        {
            //YUV422 is in index 1
            if(pixelFormat == 2)
                pixelFormat = 1;
        }
        ui->PixelFormatList->setCurrentIndex(pixelFormat);
    }
}

void MainWindow::on_PortClockSetBut_clicked()
{
    LCR_SetPortClock(ui->PortClockList->currentIndex());
}

void MainWindow::on_PortClockGetBut_clicked()
{
    unsigned int portClock;
    if(LCR_GetPortClock(&portClock) == 0)
        ui->PortClockList->setCurrentIndex(portClock);
}

void MainWindow::on_SetSwapBut_clicked()
{
    unsigned int portSelect;
    if(ui->InputSourceList->currentIndex() == 0)
        portSelect = 0;
    else if (ui->InputSourceList->currentIndex() == 3)
        portSelect = 1;
    else
        return;
    LCR_SetDataChannelSwap(portSelect, ui->SwapSelectList->currentIndex());
}

void MainWindow::on_GetSwapBut_clicked()
{
    unsigned int port, swap;
    if(LCR_GetDataChannelSwap(&port, &swap) == 0)
    {
        ui->SwapSelectList->setCurrentIndex(swap);
    }
}

void MainWindow::on_FPDModeSetBut_clicked()
{
    LCR_SetFPD_Mode_Field(ui->FPD_PixMode_spinBox->value(), ui->FPD_Inv_Pol_checkBox->isChecked(), ui->FPDFieldSelectList->currentIndex());
}

void MainWindow::on_FPDModeGetBut_clicked()
{
    unsigned int pixelMappingMode, signalSelect;
    bool pol;

    if(LCR_GetFPD_Mode_Field(&pixelMappingMode, &pol, &signalSelect) == 0)
    {
        ui->FPD_PixMode_spinBox->setValue(pixelMappingMode);
        ui->FPD_Inv_Pol_checkBox->setChecked(pol);
        ui->FPDFieldSelectList->setCurrentIndex(signalSelect);
    }
}

void MainWindow::on_TPGColorSetBut_clicked()
{
    LCR_SetTPGColor(ui->TPGRed_FG_spinBox->value(), ui->TPGGreen_FG_spinbox->value(), ui->TPGBlue_FG_spinBox->value(), ui->TPGRed_BG_spinBox->value(), ui->TPGGreen_BG_spinBox->value(), ui->TPGBlue_BG_spinBox->value());
}

void MainWindow::on_TPGColorGetBut_clicked()
{
    unsigned short redFG, greenFG, blueFG;
    unsigned short redBG, greenBG, blueBG;

    if(LCR_GetTPGColor(&redFG, &greenFG, &blueFG, &redBG, &greenBG, &blueBG) == 0)
    {
        ui->TPGRed_FG_spinBox->setValue(redFG);
        ui->TPGGreen_FG_spinbox->setValue(greenFG);
        ui->TPGBlue_FG_spinBox->setValue(blueFG);
        ui->TPGRed_BG_spinBox->setValue(redBG);
        ui->TPGGreen_BG_spinBox->setValue(greenBG);
        ui->TPGBlue_BG_spinBox->setValue(blueBG);
    }
}

void MainWindow::on_displaySetBut_clicked()
{
    rectangle croppedArea, displayArea;

    croppedArea.firstPixel = strToNum(ui->firstPixInput->text());
    croppedArea.firstLine = strToNum(ui->firstLineInput->text());
    croppedArea.pixelsPerLine = strToNum(ui->widthInput->text());
    croppedArea.linesPerFrame = strToNum(ui->heightInput->text());
    displayArea.firstPixel = strToNum(ui->firstPixInput_2->text());
    displayArea.firstLine = strToNum(ui->firstLineInput_2->text());
    displayArea.pixelsPerLine = strToNum(ui->widthInput_2->text());
    displayArea.linesPerFrame = strToNum(ui->heightInput_2->text());

    LCR_SetDisplay(croppedArea, displayArea);
}

void MainWindow::on_displayGetBut_clicked()
{
    rectangle croppedArea, displayArea;

    if(LCR_GetDisplay(&croppedArea, &displayArea) == 0)
    {
        ui->firstPixInput->setText(numToStr(croppedArea.firstPixel));
        ui->firstLineInput->setText(numToStr(croppedArea.firstLine));
        ui->widthInput->setText(numToStr(croppedArea.pixelsPerLine));
        ui->heightInput->setText(numToStr(croppedArea.linesPerFrame));

        ui->firstPixInput_2->setText(numToStr(displayArea.firstPixel));
        ui->firstLineInput_2->setText(numToStr(displayArea.firstLine));
        ui->widthInput_2->setText(numToStr(displayArea.pixelsPerLine));
        ui->heightInput_2->setText(numToStr(displayArea.linesPerFrame));
    }
}

quint32 BitPlanes[24][8] =
{
    { 0x01, 0x03, 0x07, 0x0F, 0x3E, 0x3F, 0xFE, 0xFF },  //G0 Bit-depth 1-8
    { 0x02, 0x03, 0x07, 0x0F, 0x3E, 0x3F, 0xFE, 0xFF },  //G1 Bit-depth 1-8
    { 0x04, 0x0C, 0x07, 0x0F, 0x3E, 0x3F, 0xFE, 0xFF },  //G2 Bit-depth 1-8
    { 0x08, 0x0C, 0x38, 0x0F, 0x3E, 0x3F, 0xFE, 0xFF },  //G3 Bit-depth 1-8
    { 0x10, 0x30, 0x38, 0xF0, 0x3E, 0x3F, 0xFE, 0xFF },  //G4 Bit-depth 1-8
    { 0x20, 0x30, 0x38, 0xF0, 0x3E, 0x3F, 0xFE, 0xFF },  //G5 Bit-depth 1-8
    { 0x40, 0xC0, 0x1C0, 0xF0, 0xF80, 0xFC0,0xFE, 0xFF },  //G6 Bit-depth 1-8
    { 0x80, 0xC0, 0x1C0, 0xF0, 0xF80, 0xFC0,0xFE, 0xFF }, //G7 Bit-depth 1-8


    { 0x0100, 0x0300, 0x1C0, 0xF00, 0xF80,     0xFC0,   0xFE00, 0xFF00 },  //R0 Bit-depth 1-8
    { 0x0200, 0x0300, 0xE00, 0xF00, 0xF80,     0xFC0,   0xFE00, 0xFF00 },  //R1 Bit-depth 1-8
    { 0x0400, 0x0C00, 0xE00, 0xF00, 0xF80,     0xFC0,   0xFE00, 0xFF00 },  //R2 Bit-depth 1-8
    { 0x0800, 0x0C00, 0xE00, 0xF00, 0xF80,     0xFC0,   0xFE00, 0xFF00 },  //R3 Bit-depth 1-8
    { 0x1000, 0x3000, 0x7000, 0xF000, 0x3E000,        0x3F000, 0xFE00, 0xFF00 },  //R4 Bit-depth 1-8
    { 0x2000, 0x3000, 0x7000, 0xF000, 0x3E000,  0x3F000, 0xFE00, 0xFF00 },  //R5 Bit-depth 1-8
    { 0x4000, 0xC000, 0x7000, 0xF000, 0x3E000, 0x3F000, 0xFE00, 0xFF00 },  //R6 Bit-depth 1-8
    { 0x8000, 0xC000, 0x38000, 0xF000, 0x3E000, 0x3F000, 0xFE00, 0xFF00 }, //R7 Bit-depth 1-8

    { 0x010000, 0x030000, 0x38000, 0xF0000,   0x3E000,  0x3F000,  0xFE0000, 0xFF0000 },  //B0 Bit-depth 1-8
    { 0x020000, 0x030000, 0x38000, 0xF0000,   0x3E000,  0x3F000,  0xFE0000, 0xFF0000 },  //B1 Bit-depth 1-8
    { 0x040000, 0x0C0000, 0x1C0000, 0xF0000,  0xF80000,   0xFC0000, 0xFE0000, 0xFF0000 },  //B2 Bit-depth 1-8
    { 0x080000, 0x0C0000, 0x1C0000, 0xF0000,  0xF80000, 0xFC0000, 0xFE0000, 0xFF0000 },  //B3 Bit-depth 1-8
    { 0x100000, 0x300000, 0x1C0000, 0xF00000, 0xF80000, 0xFC0000, 0xFE0000, 0xFF0000 },  //B4 Bit-depth 1-8
    { 0x200000, 0x300000, 0xE00000, 0xF00000, 0xF80000, 0xFC0000, 0xFE0000, 0xFF0000 },  //B5 Bit-depth 1-8
    { 0x400000, 0xC00000, 0xE00000, 0xF00000, 0xF80000, 0xFC0000, 0xFE0000, 0xFF0000 },  //B6 Bit-depth 1-8
    { 0x800000, 0xC00000, 0xE00000, 0xF00000, 0xF80000, 0xFC0000, 0xFE0000, 0xFF0000 },  //B7 Bit-depth 1-8
};

void MainWindow::on_listWidget_itemClicked(QListWidgetItem *item)
{
    int selectedRow = item->listWidget()->row(item);
    int i;
    unsigned int selectionMask;

    item->listWidget()->selectionModel()->clearSelection();
    selectionMask = BitPlanes[selectedRow][ui->BitDepth_combobox->currentIndex()];
    for(i=0; i < 24; i++)
    {
        if(selectionMask & 1)
        {
            item->listWidget()->item(i)->setSelected(true);
        }
        selectionMask >>= 1;
    }
}

QString MainWindow::GenerateItemText(int frame_Index, int bitDepth, int firstItem, int lastItem)
{
    QString itemText;

    if(patFromSplash)
        itemText = "S";
    else
        itemText = "F";

    itemText.append(numToStr(frame_Index));
    itemText.append("::");
    itemText.append(ui->listWidget->item(firstItem)->text());
    if(lastItem != firstItem)
    {
        itemText.append("-");
        itemText.append(ui->listWidget->item(lastItem)->text());
    }
    else
        itemText.prepend("   ");

    itemText.append("\n");
    itemText.append("  (");
    itemText.append(numToStr(bitDepth));
    itemText.append(")");

    return itemText;

}

QColor MainWindow::GetColorFromIndex(int index)
{
    QColor bgColor;

    switch(index)
    {
    case 0:
        return -1;
    case 1:
        bgColor = Qt::red;
        break;
    case 2:
        bgColor = Qt::green;
        break;
    case 3:
        bgColor = Qt::yellow;
        break;
    case 4:
        bgColor = Qt::blue;
        break;
    case 5:
        bgColor = Qt::magenta;
        break;
    case 6:
        bgColor = Qt::cyan;
        break;
    case 7:
        bgColor = Qt::white;
        break;
    }
    return bgColor;
}


void MainWindow::updateSeqItemData(int trigType, int pat_num,int bitDepth,int Led_Select,int frame_index, bool invertPat, bool insertBlack,bool bufSwap, bool TrigOutPrev)
{
    //Update the Data of the listItem with all relevant information for retrieval later.
    ui->Seq_listWidget->item(ui->Seq_listWidget->count()-1)->setData(Qt::UserRole+TrigType, trigType);
    ui->Seq_listWidget->item(ui->Seq_listWidget->count()-1)->setData(Qt::UserRole+PatNum, pat_num);
    ui->Seq_listWidget->item(ui->Seq_listWidget->count()-1)->setData(Qt::UserRole+BitDepth, bitDepth);
    ui->Seq_listWidget->item(ui->Seq_listWidget->count()-1)->setData(Qt::UserRole+LEDSelect, Led_Select);
    ui->Seq_listWidget->item(ui->Seq_listWidget->count()-1)->setData(Qt::UserRole+InvertPat, invertPat);
    ui->Seq_listWidget->item(ui->Seq_listWidget->count()-1)->setData(Qt::UserRole+InsertBlack, insertBlack);
    ui->Seq_listWidget->item(ui->Seq_listWidget->count()-1)->setData(Qt::UserRole+frameIndex, frame_index);
    ui->Seq_listWidget->item(ui->Seq_listWidget->count()-1)->setData(Qt::UserRole+BufSwap, bufSwap);
    ui->Seq_listWidget->item(ui->Seq_listWidget->count()-1)->setData(Qt::UserRole+trigOutPrev, TrigOutPrev);
}

void MainWindow::on_AddSeq_pushButton_clicked()
{
    int i;
    int firstItem=25, lastItem;
    unsigned int pat_num=0;
    unsigned int prevFrameIdx=0;
    QColor bgColor;
    QPoint triggerPos;
    bool bufSwap;
    QListWidgetItem *curItem;
    unsigned int seqListLength;
    int trigger_type;

    if(ui->changeColor_comboBox->currentIndex()==0)
    {
        showError("Select a Color");
        return;
    }

    if(ui->listWidget->selectedItems().count() != (ui->BitDepth_combobox->currentIndex()+1))
    {
            showError("Number of bits selected must equal Bit Depth");
            return;
    }

    for(i=0; i<24; i++)
    {
        if(ui->listWidget->item(i)->isSelected())
        {
            if(firstItem == 25)
                firstItem = i;
            lastItem = i;
        }
    }

    //If first item
    if(ui->Seq_listWidget->count() == 0)
    {
        if(patFromSplash)
        {
            //Trigger type can't be "No Trigger"
            if(ui->trigInProp_comboBox->currentIndex() == NoTrig)
            {
                showError("First Item must be triggered. Please select a Trigger_In_Type other than No Trigger");
                return;
            }
            bufSwap = true;//false;
        }
        else
        {
            //In streaming mode, first item has to be triggered by vsync
            bufSwap = true;
        }
    }
    else
    {
        prevFrameIdx = ui->Seq_listWidget->item(ui->Seq_listWidget->count()-1)->data(Qt::UserRole+frameIndex).toUInt();
        if(prevFrameIdx != FrameIdx)
        {
            bufSwap = true;
        }
        else
        {
            bufSwap = false;
        }
    }

    if(patFromSplash)
    {
        trigger_type = ui->trigInProp_comboBox->currentIndex();
    }
    else
    {
        if(bufSwap == true)
            trigger_type = ExtPos;
        else
            trigger_type = NoTrig;
    }

    insertTriggerItem(trigger_type);

    ui->Seq_listWidget->addItem(GenerateItemText(FrameIdx, ui->BitDepth_combobox->currentIndex()+1, firstItem, lastItem));

    seqListLength = ui->Seq_listWidget->count();
    curItem = ui->Seq_listWidget->item(seqListLength-1);

    bgColor = GetColorFromIndex(ui->changeColor_comboBox->currentIndex());
    curItem->setBackgroundColor(bgColor);

    //Update the Data of the listItem with all relevant information for retrieval later.
    pat_num = firstItem/(ui->BitDepth_combobox->currentIndex()+1);
    updateSeqItemData(trigger_type, pat_num, ui->BitDepth_combobox->currentIndex()+1, ui->changeColor_comboBox->currentIndex(),FrameIdx, 0, ui->InsBlack_checkbox->isChecked(), bufSwap, 0);
    ui->Seq_listWidget->scrollToItem(ui->Seq_listWidget->item(seqListLength-1));
}


void MainWindow::on_Seq_listWidget_itemClicked(QListWidgetItem *item)
{
#if 0
    QColor selectedItemBgColor;
    int i;

    selectedItemBgColor = item->backgroundColor();
    item->listWidget()->selectionModel()->clearSelection();

    i = item->listWidget()->row(item);
    item->setSelected(true);

    while(++i < item->listWidget()->count() && item->listWidget()->item(i)->backgroundColor().operator ==(selectedItemBgColor))
        item->listWidget()->item(i)->setSelected(true);

    i = item->listWidget()->row(item);

    while(i-- > 0 && item->listWidget()->item(i)->backgroundColor().operator ==(selectedItemBgColor))
        item->listWidget()->item(i)->setSelected(true);
#endif
}

void MainWindow::remove_Selected_from_SeqList()
{
    int i;
    unsigned int buffer_swap=0;
    bool foundSelection = false;
    bool done = true;   //Single selection only

    for(i=0; i<ui->Seq_listWidget->count(); i++)
    {
        while(ui->Seq_listWidget->item(i)->isSelected())
        {
            if(ui->Seq_listWidget->item(i)->icon().isNull()) //Selected item is not a trigger Icon
            {
                if( i > 0 && ui->Seq_listWidget->item(i-1)->icon().isNull() == false) //if the previous item is a trigger icon; remove that first
                {
                    i--;
                    done = false;
                }
            }

            buffer_swap=0;
            //Item to be removed now is a pattern
            if(ui->Seq_listWidget->item(i)->icon().isNull())
            {
                if(ui->Seq_listWidget->item(i)->data(Qt::UserRole+BufSwap) == 1)
                    buffer_swap=1;

                //if the next item is also a pattern and if it shares the exposure time with the item being removed; then remove that also
                while((i+1) < ui->Seq_listWidget->count()) //make sure we don't read past the array
                {
                    if(ui->Seq_listWidget->item(i+1)->icon().isNull())
                    {
                        if(ui->Seq_listWidget->item(i+1)->data(Qt::UserRole+trigOutPrev) == 1)
                        {
                            ui->Seq_listWidget->takeItem(i+1);
                        }
                        else
                        {
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else //Item to be removed now is a trigger
            {
                //Set trigger type to "No Trigger" when trigger is removed.
                ui->Seq_listWidget->item(i+1)->setData(Qt::UserRole+TrigType, NoTrig);
            }

            ui->Seq_listWidget->takeItem(i);
            foundSelection = true;
            if(i>=ui->Seq_listWidget->count())
            {
                ui->Seq_listWidget->selectionModel()->clearSelection();
                return;
            }
            if(buffer_swap) //If BufSwap item was removed, the bufSwap needs to be inserted in the next item
                ui->Seq_listWidget->item(i)->setData(Qt::UserRole+BufSwap, buffer_swap);

            if(done)
                break;

            done = true;
        }
    }
    if(!foundSelection)
        showError("None Selected");
    ui->Seq_listWidget->selectionModel()->clearSelection();
}

void MainWindow::on_BitDepth_combobox_currentIndexChanged(int index)
{

    int i, j;
    QColor bgColor = Qt::gray;
    int index_orig = index;

    // Adjust BitPlane_label
    if (index == 0)
    {
        ui->BitPlane_label->setText("Bit Plane");
    }
    else
    {
        ui->BitPlane_label->setText("Bit Planes");
    }

    // Adjust BitPlane_combobox
    ui->BitPlane_comboBox->clear();
    switch(index)
    {
        case 0:     // Bit Depth = 1
            ui->BitPlane_comboBox->addItem("G0");
            ui->BitPlane_comboBox->addItem("G1");
            ui->BitPlane_comboBox->addItem("G2");
            ui->BitPlane_comboBox->addItem("G3");
            ui->BitPlane_comboBox->addItem("G4");
            ui->BitPlane_comboBox->addItem("G5");
            ui->BitPlane_comboBox->addItem("G6");
            ui->BitPlane_comboBox->addItem("G7");
            ui->BitPlane_comboBox->addItem("R0");
            ui->BitPlane_comboBox->addItem("R1");
            ui->BitPlane_comboBox->addItem("R2");
            ui->BitPlane_comboBox->addItem("R3");
            ui->BitPlane_comboBox->addItem("R4");
            ui->BitPlane_comboBox->addItem("R5");
            ui->BitPlane_comboBox->addItem("R6");
            ui->BitPlane_comboBox->addItem("R7");
            ui->BitPlane_comboBox->addItem("B0");
            ui->BitPlane_comboBox->addItem("B1");
            ui->BitPlane_comboBox->addItem("B2");
            ui->BitPlane_comboBox->addItem("B3");
            ui->BitPlane_comboBox->addItem("B4");
            ui->BitPlane_comboBox->addItem("B5");
            ui->BitPlane_comboBox->addItem("B6");
            ui->BitPlane_comboBox->addItem("B7");
            break;
        case 1:     // Bit Depth = 2
            ui->BitPlane_comboBox->addItem("G1,G0");
            ui->BitPlane_comboBox->addItem("G3,G2");
            ui->BitPlane_comboBox->addItem("G5,G4");
            ui->BitPlane_comboBox->addItem("G7,G6");
            ui->BitPlane_comboBox->addItem("R1,R0");
            ui->BitPlane_comboBox->addItem("R3,R2");
            ui->BitPlane_comboBox->addItem("R5,R4");
            ui->BitPlane_comboBox->addItem("R7,R6");
            ui->BitPlane_comboBox->addItem("B1,B0");
            ui->BitPlane_comboBox->addItem("B3,B2");
            ui->BitPlane_comboBox->addItem("B5,B4");
            ui->BitPlane_comboBox->addItem("B7,B6");
            break;
        case 2:     // Bit Depth = 3
            ui->BitPlane_comboBox->addItem("G2,G1,G0");
            ui->BitPlane_comboBox->addItem("G5,G4,G3");
            ui->BitPlane_comboBox->addItem("R0,G7,G6");
            ui->BitPlane_comboBox->addItem("R3,R2,R1");
            ui->BitPlane_comboBox->addItem("R6,R5,R4");
            ui->BitPlane_comboBox->addItem("B1,B0,R7");
            ui->BitPlane_comboBox->addItem("B4,B3,B2");
            ui->BitPlane_comboBox->addItem("B7,B6,B5");
            break;
        case 3:     // Bit Depth = 4
            ui->BitPlane_comboBox->addItem("G3,G2,G1,G0");
            ui->BitPlane_comboBox->addItem("G7,G6,G5,G4");
            ui->BitPlane_comboBox->addItem("R3,R2,R1,R0");
            ui->BitPlane_comboBox->addItem("R7,R6,R5,R4");
            ui->BitPlane_comboBox->addItem("B3,B2,B1,B0");
            ui->BitPlane_comboBox->addItem("B7,B6,B5,B4");
            break;
        case 4:     // Bit Depth = 5
            ui->BitPlane_comboBox->addItem("G5,G4,G3,G2,G1");
            ui->BitPlane_comboBox->addItem("R3,R2,R1,R0,G7");
            ui->BitPlane_comboBox->addItem("B1,B0,R7,R6,R5");
            ui->BitPlane_comboBox->addItem("B7,B6,B5,B4,B3");
            break;
        case 5:     // Bit Depth = 6
            ui->BitPlane_comboBox->addItem("G5,G4,G3,G2,G1,G0");
            ui->BitPlane_comboBox->addItem("R3,R2,R1,R0,G7,G6");
            ui->BitPlane_comboBox->addItem("B1,B0,R7,R6,R5,R4");
            ui->BitPlane_comboBox->addItem("B7,B6,B5,B4,B3,B2");
            break;
        case 6:     // Bit Depth = 7
            ui->BitPlane_comboBox->addItem("G7,G6,G5,G4,G3,G2,G1");
            ui->BitPlane_comboBox->addItem("R7,R6,R5,R4,R3,R2,R1");
            ui->BitPlane_comboBox->addItem("B7,B6,B5,B4,B3,B2,B1");
            break;
        case 7:     // Bit Depth = 8
            ui->BitPlane_comboBox->addItem("G7,G6,G5,G4,G3,G2,G1,G0");
            ui->BitPlane_comboBox->addItem("R7,R6,R5,R4,R3,R2,R1,R0");
            ui->BitPlane_comboBox->addItem("B7,B6,B5,B4,B3,B2,B1,B0");
            break;
    }


    //////////////////////////////////////////////////////////////////////////////////////////
    if(index == 4 || index == 6)
        index +=1;

    ui->listWidget->selectionModel()->clearSelection();

    for(i=0; i<24;)
    {
        for(j=0; j<=index; j++,i++)
        {
            ui->listWidget->item(i)->setBackgroundColor(bgColor);
        }
        if(bgColor == Qt::gray)
            bgColor = Qt::lightGray;
        else
            bgColor = Qt::gray;
    }

    if(index_orig == 4)  //Special case of bit-depth 5
    {
        ui->listWidget->item(0)->setBackgroundColor(Qt::black);
        ui->listWidget->item(6)->setBackgroundColor(Qt::black);
        ui->listWidget->item(12)->setBackgroundColor(Qt::black);
        ui->listWidget->item(18)->setBackgroundColor(Qt::black);
    }

    if(index_orig == 6)  //Special case of bit-depth 7
    {
        ui->listWidget->item(0)->setBackgroundColor(Qt::black);
        ui->listWidget->item(8)->setBackgroundColor(Qt::black);
        ui->listWidget->item(16)->setBackgroundColor(Qt::black);
    }
    //////////////////////////////////////////////////////////////////////////////////////////
}

void MainWindow::on_clearAllButton_clicked()
{
    numExtraSplashLutEntries = 0;

    while(ui->Seq_listWidget->count() != 0)
        ui->Seq_listWidget->takeItem(0);
    ui->frame_splash_index->setValue(0);
    updateFrameTitle();
}

void MainWindow::updateFrameTitle()
{
    if(patFromSplash)
        ui->frame_splash_label->setText("Flash Index");
    else
        ui->frame_splash_label->setText("Frame Index");
}

void MainWindow::combineItemWithPrev(unsigned int itemRow)
{
    unsigned int lastVisibleItem;

    if(itemRow == 0)
        return;

    //Return if prev item is trigger item
    if(ui->Seq_listWidget->item(itemRow-1)->icon().isNull() == false)
        return;

    lastVisibleItem = itemRow-1;

    while(ui->Seq_listWidget->item(lastVisibleItem)->isHidden())
        lastVisibleItem--;

    //Append color name to the end of prev item text
    if (ui->Seq_listWidget->item(lastVisibleItem)->backgroundColor() != Qt::gray)
    {
        switch (ui->Seq_listWidget->item(lastVisibleItem)->data(Qt::UserRole+LEDSelect).toUInt())
        {
        case 1:
            ui->Seq_listWidget->item(lastVisibleItem)->setText(ui->Seq_listWidget->item(itemRow-1)->text().append("Red"));;
            break;
        case 2:
            ui->Seq_listWidget->item(lastVisibleItem)->setText(ui->Seq_listWidget->item(itemRow-1)->text().append("Green"));;
            break;
        case 3:
            ui->Seq_listWidget->item(lastVisibleItem)->setText(ui->Seq_listWidget->item(itemRow-1)->text().append("Yellow"));;
            break;
        case 4:
            ui->Seq_listWidget->item(lastVisibleItem)->setText(ui->Seq_listWidget->item(itemRow-1)->text().append("Blue"));;
            break;
        case 5:
            ui->Seq_listWidget->item(lastVisibleItem)->setText(ui->Seq_listWidget->item(itemRow-1)->text().append("Magenta"));;
            break;
        case 6:
            ui->Seq_listWidget->item(lastVisibleItem)->setText(ui->Seq_listWidget->item(itemRow-1)->text().append("Cyan"));;
            break;
        case 7:
            ui->Seq_listWidget->item(lastVisibleItem)->setText(ui->Seq_listWidget->item(itemRow-1)->text().append("White"));;
            break;
        }
    }

    //Append color name to the end of selected item text
    if (ui->Seq_listWidget->item(itemRow)->backgroundColor() != Qt::gray)
    {
        switch (ui->Seq_listWidget->item(itemRow)->data(Qt::UserRole+LEDSelect).toUInt())
        {
        case 1:
            ui->Seq_listWidget->item(itemRow)->setText(ui->Seq_listWidget->item(itemRow)->text().append("Red"));;
            break;
        case 2:
            ui->Seq_listWidget->item(itemRow)->setText(ui->Seq_listWidget->item(itemRow)->text().append("Green"));;
            break;
        case 3:
            ui->Seq_listWidget->item(itemRow)->setText(ui->Seq_listWidget->item(itemRow)->text().append("Yellow"));;
            break;
        case 4:
            ui->Seq_listWidget->item(itemRow)->setText(ui->Seq_listWidget->item(itemRow)->text().append("Blue"));;
            break;
        case 5:
            ui->Seq_listWidget->item(itemRow)->setText(ui->Seq_listWidget->item(itemRow)->text().append("Magenta"));;
            break;
        case 6:
            ui->Seq_listWidget->item(itemRow)->setText(ui->Seq_listWidget->item(itemRow)->text().append("Cyan"));;
            break;
        case 7:
            ui->Seq_listWidget->item(itemRow)->setText(ui->Seq_listWidget->item(itemRow)->text().append("White"));;
            break;
        }
    }

    ui->Seq_listWidget->item(lastVisibleItem)->setText(ui->Seq_listWidget->item(lastVisibleItem)->text().append("\n"));
    ui->Seq_listWidget->item(lastVisibleItem)->setText(ui->Seq_listWidget->item(lastVisibleItem)->text().append(ui->Seq_listWidget->item(itemRow)->text()));
    ui->Seq_listWidget->item(itemRow)->setData(Qt::UserRole+trigOutPrev, 1);
    ui->Seq_listWidget->item(lastVisibleItem)->setBackgroundColor(Qt::gray);
    ui->Seq_listWidget->setRowHidden(itemRow, true);
    ui->Seq_listWidget->selectionModel()->clearSelection();

    return;
}


void MainWindow::on_Seq_listWidget_customContextMenuRequested(const QPoint &pos)
{
    int itemRow=0;
    QMenu seqListMenu(this);

    QAction *removeAct;
    QAction *invertAct;
    QAction *insertAct = new QAction(tr("Insert Trigger-In"), this);
    QAction *combineAct = new QAction(tr("Share exposure time with prev pattern"), this);
    QAction *toggleInsBlkAct = new QAction(tr("Clear DMD after exposure time"), this);

    toggleInsBlkAct->setCheckable(true);

    itemRow = ui->Seq_listWidget->row((ui->Seq_listWidget->itemAt(pos)));
    if(itemRow < 0 || itemRow > 127)
        return;

    if(!ui->Seq_listWidget->item(itemRow)->icon().isNull())
    {
        invertAct = new QAction(tr("Invert Trigger-In"), this);
        removeAct = new QAction("Remove Trigger-In", this);
    }
    else
    {
        invertAct = new QAction(tr("Invert Pattern"), this);
        removeAct = new QAction("Remove Pattern", this);
    }

     //For trigger Items
    if(!ui->Seq_listWidget->item(itemRow)->icon().isNull())
    {
        //Invert option not applicable for Internal trigger. Also, for straming mode, external trigger can only be positive.
        if(ui->Seq_listWidget->item(itemRow+1)->data(Qt::UserRole+TrigType).toInt() != IntTrig && patFromSplash == 1)
            seqListMenu.addAction(invertAct);
    }
    else
    {
        if(itemRow < ui->Seq_listWidget->count()-1)//if not last item
        {
            //check if next items are combined (hidden/trigOutPrev=1) with this one. If yes, don't give the option to invert this item
            if(!ui->Seq_listWidget->item(itemRow+1)->isHidden())
            {
                seqListMenu.addAction(invertAct);
            }

        }
        else
            seqListMenu.addAction(invertAct);

        //If not first item and previous item is also a pattern item
        if(itemRow != 0 && ui->Seq_listWidget->item(itemRow-1)->icon().isNull())
        {
            seqListMenu.addAction(combineAct);
        }
        seqListMenu.addAction(toggleInsBlkAct);
        if(ui->Seq_listWidget->item(itemRow)->data(Qt::UserRole+InsertBlack).toInt() != 0)
            toggleInsBlkAct->setChecked(true);
        else
            toggleInsBlkAct->setChecked(false);
    }

    if(!ui->Seq_listWidget->item(itemRow)->icon().isNull() && itemRow == 0)
    {
        /* First trigger item cannot be removed */
    }
    else
    {
        seqListMenu.addAction(removeAct);
    }

    if(patFromSplash)
    {
        //If pattern item and trigger type is "No Trigger"
        if(ui->Seq_listWidget->item(itemRow)->icon().isNull() && ui->Seq_listWidget->item(itemRow)->data(Qt::UserRole+TrigType).toInt() == NoTrig)
        {
            seqListMenu.addAction(insertAct);
        }
    }

    QAction* selectedAct = seqListMenu.exec(QCursor::pos());

    if(selectedAct == removeAct)
    {
        remove_Selected_from_SeqList();
    }
    else if(selectedAct == invertAct)
    {
        if(!ui->Seq_listWidget->item(itemRow)->icon().isNull()) //Invert Trigger-In
        {
            if(ui->Seq_listWidget->item(itemRow+1)->data(Qt::UserRole+TrigType).toInt() == ExtPos)
            {
                ui->Seq_listWidget->item(itemRow+1)->setData(Qt::UserRole+TrigType, ExtNeg);
                QIcon trigInvIcon (":/new/prefix1/Icons/trigger_inv.png");
                ui->Seq_listWidget->item(itemRow)->setIcon(trigInvIcon);
            }
            else if(ui->Seq_listWidget->item(itemRow+1)->data(Qt::UserRole+TrigType).toInt() == ExtNeg)
            {
                ui->Seq_listWidget->item(itemRow+1)->setData(Qt::UserRole+TrigType, ExtPos);
                QIcon trigInvIcon (":/new/prefix1/Icons/trigger.png");
                ui->Seq_listWidget->item(itemRow)->setIcon(trigInvIcon);
            }
        }
        else //Invert Pattern
        {
            if(ui->Seq_listWidget->item(itemRow)->text().contains("~"))
            {
                QString itemText = ui->Seq_listWidget->item(itemRow)->text();
                itemText.chop(6);
                ui->Seq_listWidget->item(itemRow)->setText(itemText);
                ui->Seq_listWidget->item(itemRow)->setData(Qt::UserRole+InvertPat, 0);
            }
            else
            {
                ui->Seq_listWidget->item(itemRow)->setText(ui->Seq_listWidget->item(itemRow)->text().append("\n    ~"));
                ui->Seq_listWidget->item(itemRow)->setData(Qt::UserRole+InvertPat, 1);
            }
        }
    }
    else if(selectedAct == insertAct)
    {
        if(ui->trigInProp_comboBox->currentIndex() == ExtNeg)
        {
            ui->Seq_listWidget->item(itemRow)->setData(Qt::UserRole+TrigType, ExtNeg);
            ui->Seq_listWidget->insertItem(itemRow, "");
            QIcon trigIcon (":/new/prefix1/Icons/trigger_inv.png");
            ui->Seq_listWidget->item(itemRow)->setIcon(trigIcon);
        }
        else
        {
            if(ui->trigInProp_comboBox->currentIndex() == ExtPos)
                ui->Seq_listWidget->item(itemRow)->setData(Qt::UserRole+TrigType, ExtPos);
            else if(ui->trigInProp_comboBox->currentIndex() == IntTrig)
                ui->Seq_listWidget->item(itemRow)->setData(Qt::UserRole+TrigType, IntTrig);
            else
            {
                showError("Please select trigger type to be inserted using Trigger_In_Type combobox");
                return;
            }
            ui->Seq_listWidget->insertItem(itemRow, "");
            QIcon trigIcon (":/new/prefix1/Icons/trigger.png");
            ui->Seq_listWidget->item(itemRow)->setIcon(trigIcon);
        }
    }
    else if(selectedAct == combineAct)
    {
        combineItemWithPrev(itemRow);
    }
    else if(selectedAct == toggleInsBlkAct)
    {
        if(toggleInsBlkAct->isChecked())
            ui->Seq_listWidget->item(itemRow)->setData(Qt::UserRole+InsertBlack, 1);
        else
            ui->Seq_listWidget->item(itemRow)->setData(Qt::UserRole+InsertBlack, 0);
    }
}


void MainWindow::on_sendLUT_pushButton_clicked()
{
    int i, numLutEntries=0;
    unsigned int status;
    char errorStr[256];
    unsigned char splashLut[64];
    int numSplashLutEntries = 0;
    int num_pats_in_exposure=1;
    unsigned int min_pat_exposure[8] = {235, 700, 1570, 1700, 2000, 2500, 4500, 8333};
    unsigned int worstCaseBitDepth = 0;
    unsigned int numPatterns;
    unsigned int total_exposure_time=0;
    unsigned int rogueExposureTime=0;
    bool displayTimeLoadTimeMismatch=false;

    if(ui->Seq_listWidget->count() == 0)
        return;

    LCR_ClearPatLut();
    for(i=0; i<ui->Seq_listWidget->count(); i++)
    {
        if(ui->Seq_listWidget->item(i)->icon().isNull()) //only if pattern item (skip trigger items)
        {
            if(i==0)
            {
                showError("First Item must be triggered. Please select a Trigger_In_Type other than No Trigger");
                return;
            }

            /*********************Expsosure time validation logic begin *****************/
            //If trigOut = NEW
            if(ui->Seq_listWidget->item(i)->data(Qt::UserRole+trigOutPrev).toBool() == false)
            {
                if(num_pats_in_exposure != 1)
                {
                    //Check if expsoure time is above the minimum requirement
                    if(ui->patExp_lineEdit->text().toUInt()/num_pats_in_exposure < min_pat_exposure[worstCaseBitDepth])
                    {
                        sprintf(errorStr, "Exposure time %d < Minimum Exposure time %d for bit depth %d", ui->patExp_lineEdit->text().toUInt()/num_pats_in_exposure, min_pat_exposure[worstCaseBitDepth], worstCaseBitDepth+1);
                        showError(errorStr);
                        return;
                    }
                }
                if(ui->Seq_listWidget->item(i)->data(Qt::UserRole+BitDepth).toUInt()-1 > worstCaseBitDepth)
                {
                    worstCaseBitDepth = ui->Seq_listWidget->item(i)->data(Qt::UserRole+BitDepth).toInt()-1;
                }
                num_pats_in_exposure=1;
                worstCaseBitDepth = 0;
            }
            else //if trigOut = PREV
            {
                num_pats_in_exposure++;
                if(ui->Seq_listWidget->item(i)->data(Qt::UserRole+BitDepth).toUInt()-1 > worstCaseBitDepth)
                    worstCaseBitDepth = ui->Seq_listWidget->item(i)->data(Qt::UserRole+BitDepth).toInt()-1;
            }

            if(ui->Seq_listWidget->item(i)->data(Qt::UserRole+BufSwap).toBool() == true)
            {
                if(total_exposure_time != 0)
                {
                    if(patFromSplash)
                    {
                        if(total_exposure_time < ui->framePer_lineEdit->text().toUInt())
                        {
                            displayTimeLoadTimeMismatch = true;
                            rogueExposureTime = total_exposure_time;
                        }
                    }
                    else
                    {
                        if(total_exposure_time > ui->framePer_lineEdit->text().toUInt())
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

            if(LCR_AddToPatLut(ui->Seq_listWidget->item(i)->data(Qt::UserRole+TrigType).toInt(), \
                            ui->Seq_listWidget->item(i)->data(Qt::UserRole+PatNum).toInt(), \
                            ui->Seq_listWidget->item(i)->data(Qt::UserRole+BitDepth).toInt(), \
                            ui->Seq_listWidget->item(i)->data(Qt::UserRole+LEDSelect).toInt(), \
                            ui->Seq_listWidget->item(i)->data(Qt::UserRole+InvertPat).toBool(), \
                            ui->Seq_listWidget->item(i)->data(Qt::UserRole+InsertBlack).toBool(), \
                            ui->Seq_listWidget->item(i)->data(Qt::UserRole+BufSwap).toBool(), \
                            ui->Seq_listWidget->item(i)->data(Qt::UserRole+trigOutPrev).toBool()) < 0)
            {
                showError("Error Updating LUT");
                return;
            }
            //If there is a buffer swap or if this is the first pattern
            if((ui->Seq_listWidget->item(i)->data(Qt::UserRole+BufSwap).toBool()) || (numSplashLutEntries == 0))
            {
                if (numSplashLutEntries >= 64)
                    showError("Image LUT entries(64) reached maximum. Will not add anymore entries\n");
                else
                    splashLut[numSplashLutEntries++] = ui->Seq_listWidget->item(i)->data(Qt::UserRole+frameIndex).toInt();
            }
            numLutEntries++;
        }
    }

    if(total_exposure_time != 0)
    {
        if(patFromSplash)
        {
            if(total_exposure_time < ui->framePer_lineEdit->text().toUInt())
            {
                displayTimeLoadTimeMismatch = true;
                rogueExposureTime = total_exposure_time;
            }
        }
        else
        {
            if(total_exposure_time > ui->framePer_lineEdit->text().toUInt())
            {
                sprintf(errorStr, "Buffer display time %d > Vsync period %d. Please correct it and try again.", numLutEntries*ui->patExp_lineEdit->text().toUInt(), ui->framePer_lineEdit->text().toUInt());
                showError(errorStr);
                return;
            }

        }
    }

    if(displayTimeLoadTimeMismatch == true && (numSplashLutEntries > 2))
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

    if(num_pats_in_exposure != 1)
    {
        //Check if expsoure time is above the minimum requirement
        if(ui->patExp_lineEdit->text().toUInt()/num_pats_in_exposure < min_pat_exposure[worstCaseBitDepth])
        {
            sprintf(errorStr, "Exposure time %d < Minimum Exposure time %d for bit depth %d", ui->patExp_lineEdit->text().toUInt()/num_pats_in_exposure, min_pat_exposure[worstCaseBitDepth], worstCaseBitDepth+1);
            showError(errorStr);
            return;
        }
    }

    LCR_SetPatternDisplayMode(ui->patFrom_video->isChecked());

    //if play once is selected
    if(ui->patDispMode_radioButton->isChecked())
    {
        numPatterns = numLutEntries;
    }
    else
    {
        numPatterns = ui->PatNum_spinBox->value();
    }

    if(LCR_SetPatternConfig(numLutEntries, ui->patDispMode_radioButton_1->isChecked() , numPatterns, numSplashLutEntries) < 0)
    {
        showError("Error Sending Pattern Config");
        return;
    }
    if(LCR_SetExposure_FramePeriod(ui->patExp_lineEdit->text().toInt(), ui->trigPeriod_lineEdit->text().toInt()) < 0)
    {
        showError("Error Sending Exposure period");
        return;
    }
    if(LCR_SetPatternTriggerMode(ui->IntExtTrigMode_But->isChecked()) < 0)
    {
        showError("Error Sending trigger Mode");
        return;
    }

    if(LCR_SendPatLut() < 0)
    {
        showError("Error Sending Pattern LUT");
        return;
    }
    if(LCR_SendSplashLut(&splashLut[0], numSplashLutEntries) < 0)
    {
        showError("Error Sending Image LUT");
        return;
    }
    if(LCR_ValidatePatLutData(&status) < 0)
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
    if(LCR_PatternDisplay(2) < 0) //Start pattern display
    {
        showError("Error starting pattern display");
        return;
    }

}


void MainWindow::on_ReadLCr_pushButton_clicked()
{
    unsigned int exposure, framePeriod, i, j;
    bool trigMode, patDispMode;
    unsigned int numLutEntries, numPatsForTrigOut2, numSplash;
    bool repeat;
    int trig_type, Pat_Num, Bit_Depth, LED_Select, Frame_Index;
    bool Invert_Pat, Insert_Black, Buf_Swap, TrigOutPrev;
    QString itemText = "F";
    unsigned int firstItem=32, lastItem, index;
    QColor bgColor;
    unsigned char splashLut[64];
    bool patFromVideo=false;
    int patLutBytesRead, numLUTEntriesRead;
    char dbgMsg[256];

    if(LCR_GetExposure_FramePeriod(&exposure, &framePeriod) == 0)
    {
        ui->patExp_lineEdit->setText(numToStr(exposure));
        ui->trigPeriod_lineEdit->setText(numToStr(framePeriod));
    }
    else
    {
        showError("Could not read from target. Please check the connection");
        return;
    }

    emit on_clearAllButton_clicked();

    if(LCR_GetPatternTriggerMode(&trigMode) == 0)
        ui->IntExtTrigMode_But->setChecked(trigMode);

    if(LCR_GetPatternDisplayMode(&patDispMode) == 0)
    {
	if (patDispMode)
            ui->patFrom_video->setChecked(true);
	else
        ui->patFrom_flash->setChecked(true);
    }

    if(LCR_GetPatternConfig(&numLutEntries, &repeat, &numPatsForTrigOut2, &numSplash) == 0)
	{
		if(repeat == true)
		{
			ui->patDispMode_radioButton_1->setChecked(true);
			ui->PatNum_spinBox->setValue(numPatsForTrigOut2);
		}
		else
		{
			ui->patDispMode_radioButton->setChecked(true);
		}
	}

    if((patLutBytesRead = LCR_GetPatLut(numLutEntries)) < 0)
    {
        showError("Could not read pattern LUT from target");
        return;
    }

    if(patLutBytesRead % 3 == 0)
        numLUTEntriesRead = patLutBytesRead/3+1;
    else
        numLUTEntriesRead = patLutBytesRead/3;

    if (numLUTEntriesRead != numLutEntries)
    {
        sprintf(dbgMsg, "Only %d pattern LUT entries read back correctly. This issue will be fixed in the next release of firmware", numLUTEntriesRead);
        numLutEntries = numLUTEntriesRead;
        showError(dbgMsg);
    }

    if(LCR_GetSplashLut(&splashLut[0], numSplash) < 0)
    {
        showError("Could not read image LUT from target");
        return;
    }
    else
    {
        // Values read correctly so check for special 2 numSplash case and adjust so GUI displays correctly
        if(numSplash == 2)
        {
            unsigned char temp_val = splashLut[0];
            splashLut[0] = splashLut[1];
            splashLut[1] = temp_val;
        }
    }

    Frame_Index = -1;
    LCR_GetPatternDisplayMode(&patFromVideo);

    for(i=0; i<numLutEntries; i++)
    {
        LCR_GetPatLutItem(i, &trig_type, &Pat_Num, &Bit_Depth, &LED_Select, &Invert_Pat, &Insert_Black, &Buf_Swap, &TrigOutPrev);
        if(Bit_Depth <= 0 || Bit_Depth > 8)
        {
            showError("Received unexpected value for Bit depth");
            break;
        }
        insertTriggerItem(trig_type);
        if((Buf_Swap) || (i==0))
            Frame_Index++;

        firstItem = 32;
        if(Bit_Depth == 5 || Bit_Depth == 7)
            index = Pat_Num*(Bit_Depth+1);
        else
            index = Pat_Num * Bit_Depth;

        for(j=0; j < 32; j++)
        {
            if(firstItem > 31) //first set bit not found yet
            {
                if( (BitPlanes[index][Bit_Depth-1] & (1 << j)) == (1<<j))
                {
                    firstItem = j;
                }
            }
            else if((BitPlanes[index][Bit_Depth-1] & (1 << j)) == 0)
            {
                break;
            }
        }

        lastItem = j-1;

        if(patFromVideo)
            ui->Seq_listWidget->addItem(GenerateItemText(Frame_Index, Bit_Depth, firstItem, lastItem));
        else
            ui->Seq_listWidget->addItem(GenerateItemText(splashLut[Frame_Index], Bit_Depth, firstItem, lastItem));

        if(LED_Select <= 0 || LED_Select > 7)
        {
            showError("Received unexpected value for Color selection");
            break;
        }
        bgColor = GetColorFromIndex(LED_Select);
        ui->Seq_listWidget->item(ui->Seq_listWidget->count()-1)->setBackgroundColor(bgColor);

        if(patFromVideo)
            updateSeqItemData(trig_type, Pat_Num, Bit_Depth, LED_Select, Frame_Index, Invert_Pat, Insert_Black, Buf_Swap, TrigOutPrev);
        else
            updateSeqItemData(trig_type, Pat_Num, Bit_Depth, LED_Select, splashLut[Frame_Index], Invert_Pat, Insert_Black, Buf_Swap, TrigOutPrev);

        if(ui->Seq_listWidget->count() > 1)
        {
            if(ui->Seq_listWidget->item(ui->Seq_listWidget->count()-1)->data(Qt::UserRole+trigOutPrev).toBool() == true)
            {
                combineItemWithPrev(ui->Seq_listWidget->count()-1);
            }
        }
    }

}


int MainWindow::insertTriggerItem(int trig_type)
{
    QIcon trigIcon;

    //If not "No Trigger"
    if(trig_type != NoTrig)
    {
        ui->Seq_listWidget->addItem("");
        if(trig_type != ExtNeg)
        {
            trigIcon.addFile(":/new/prefix1/Icons/trigger.png");
        }
        else
        {
           trigIcon.addFile(":/new/prefix1/Icons/trigger_inv.png");
        }
        ui->Seq_listWidget->item(ui->Seq_listWidget->count()-1)->setIcon(trigIcon);
        return 0;
    }
    return -1;
}

void MainWindow::on_Play_pushButton_clicked()
{
    LCR_PatternDisplay(2);
}

void MainWindow::on_Pause_pushButton_clicked()
{
    LCR_PatternDisplay(1);
}

void MainWindow::on_Stop_pushButton_clicked()
{
    LCR_PatternDisplay(0);
}

void MainWindow::on_Trig1Out_R_spinBox_valueChanged(int arg1)
{
    char displayStr[255];
    float val;

    val = (arg1-187)*107.2;

    sprintf(displayStr, "%2.2f us", val/1000);
    ui->Trig1Out_R_val->setText(displayStr);
    ui->Trig1Out_R_Slider->setValue(arg1);
}

void MainWindow::on_Trig1Out_R_Slider_valueChanged(int value)
{
    ui->Trig1Out_R_spinBox->setValue(value);
}

void MainWindow::on_Trig1Out_F_spinBox_valueChanged(int arg1)
{
    char displayStr[255];
    float val;

    val = (arg1-187)*107.2;

    sprintf(displayStr, "%2.2f us", val/1000);
    ui->Trig1Out_F_val->setText(displayStr);
    ui->Trig1Out_F_Slider->setValue(arg1);
}

void MainWindow::on_Trig1Out_F_Slider_valueChanged(int value)
{
    ui->Trig1Out_F_spinBox->setValue(value);
}

void MainWindow::on_Trig2Out_R_spinBox_valueChanged(int arg1)
{
    char displayStr[255];
    float val;

    val = (arg1-187)*107.2;

    sprintf(displayStr, "%2.2f us", val/1000);
    ui->Trig2Out_R_val->setText(displayStr);
    ui->Trig2Out_R_Slider->setValue(arg1);
}

void MainWindow::on_Trig2Out_R_Slider_valueChanged(int value)
{
    ui->Trig2Out_R_spinBox->setValue(value);
}

void MainWindow::on_TrigOutSet_pushButton_clicked() //Also sends trig in1delay
{
    // Send trigger 1 input delay
    LCR_SetTrigIn1Delay(ui->TrigIn1_spinBox->value());

    // Send trigger 1 output delays
    LCR_SetTrigOutConfig(1, ui->InvertTrig1Out_checkBox->isChecked(), ui->Trig1Out_R_spinBox->value(), ui->Trig1Out_F_spinBox->value());

    // Send trigger 2 output delay
    LCR_SetTrigOutConfig(2, ui->InvertTrig2Out_checkBox->isChecked(), ui->Trig2Out_R_spinBox->value(), 0);
}

void MainWindow::on_TrigOutGet_pushButton_clicked()//Also gets trig in1delay
{
    unsigned int risingDelay, fallingDelay;
    unsigned int delay;
    bool invert;

    // Get trigger 1 output delays
    if(LCR_GetTrigOutConfig(1, &invert,&risingDelay, &fallingDelay) == 0)
    {
        ui->InvertTrig1Out_checkBox->setChecked(invert);
        ui->Trig1Out_R_spinBox->setValue(risingDelay);
        ui->Trig1Out_F_spinBox->setValue(fallingDelay);
    }

    // Get trigger 2 output delay
    if(LCR_GetTrigOutConfig(2, &invert,&risingDelay, &fallingDelay) == 0)
    {
        ui->InvertTrig2Out_checkBox->setChecked(invert);
        ui->Trig2Out_R_spinBox->setValue(risingDelay);
    }

    // Get trigger 1 input delay
    if(LCR_GetTrigIn1Delay(&delay) == 0)
    {
        ui->TrigIn1_spinBox->setValue(delay);
    }
}


void MainWindow::on_TrigIn1_spinBox_valueChanged(int arg1)
{
    char displayStr[255];
    float val;

    val = (arg1-187)*107.2;

    sprintf(displayStr, "%2.2f us", val/1000);
    ui->TrigIn1_val->setText(displayStr);
    ui->TrigIn1_Slider->setValue(arg1);
}

void MainWindow::on_TrigIn1_Slider_valueChanged(int value)
{
    ui->TrigIn1_spinBox->setValue(value);
}


void MainWindow::on_InvertData_checkBox_toggled(bool checked)
{
    LCR_SetInvertData(checked);
}

void MainWindow::on_pwmSet_pushButton_clicked()
{
    unsigned int channel;

    if(ui->pwmChannel_comboBox->currentIndex() != 0)
        channel = 2;
    else
        channel = 0;

    LCR_SetPWMConfig(channel, ui->PWM_Period_lineEdit->text().toUInt(), ui->pwmDutyCycle_spinBox->value());
    LCR_SetPWMEnable(channel, ui->pwmEnable_checkBox->isChecked());
}

void MainWindow::on_pwmGet_pushButton_clicked()
{
    unsigned int channel, pulsePeriod, dutyCycle;
    bool enable;

    if(ui->pwmChannel_comboBox->currentIndex() != 0)
        channel = 2;
    else
        channel = 0;

    if(LCR_GetPWMConfig(channel, &pulsePeriod,&dutyCycle) == 0)
    {
        ui->PWM_Period_lineEdit->setText(numToStr(pulsePeriod));
        ui->pwmDutyCycle_spinBox->setValue(dutyCycle);
    }
    if(LCR_GetPWMEnable(channel, &enable) == 0)
        ui->pwmEnable_checkBox->setChecked(enable);
}

void MainWindow::on_pwmSet_pushButton_2_clicked()
{
    LCR_SetPWMCaptureConfig(ui->pwmChannel_comboBox_2->currentIndex(), ui->pwmEnable_checkBox_2->isChecked(), ui->PWM_SampleRate_lineEdit->text().toUInt());
}

void MainWindow::on_pwmGet_pushButton_2_clicked()
{
    bool enabled;
    unsigned int sampleRate;

    if(LCR_GetPWMCaptureConfig(ui->pwmChannel_comboBox_2->currentIndex(), &enabled, &sampleRate) == 0)
    {
        ui->pwmEnable_checkBox_2->setChecked(enabled);
        ui->PWM_SampleRate_lineEdit->setText(numToStr(sampleRate));
    }
}

void MainWindow::on_PWMRead_pushButton_clicked()
{
    unsigned int lowPeriod, highPeriod, dutyCycle;

    if(LCR_PWMCaptureRead(ui->pwmChannel_comboBox_2->currentIndex(), &lowPeriod, &highPeriod) == 0)
    {
        if((highPeriod + lowPeriod) == 0)
            dutyCycle = 0;
        else
            dutyCycle = highPeriod*100/(highPeriod + lowPeriod);
        ui->PWM_dutyCycle_lineEdit->setText(numToStr(dutyCycle));
    }
}

void MainWindow::on_GPIOSetAlt_pushButton_clicked()
{
    bool alternativeMode = false;

    switch ( ui->GPIO_Pin_comboBox->currentIndex() )
    {
    /* LCR functions, it will just return, no modification allowed */
    case 1  :
    case 3  :
    case 4  :
    case 7  :
    case 8  :
    case 9  :
    case 10 :
    case 16 :
    case 17 :
    case 18 :
    case 19 :
    case 22 :
    case 23 :
    case 26 :
    case 31 :
    case 32 :
    case 37 :
        showError("Modification not permitted on this pin");
        return;
    /* only GPIO functions , if asking for alternate, return */
    case 13:
    case 14:
    case 15:
    case 20:
    case 21:
    case 24:
    case 25:
    case 27:
    case 28:
    case 29:
    case 30:
    case 33:
    case 34:
    case 35:
    case 36:
        if (  ui->GPIO_EnAlt_checkBox->isChecked() == true )
        {
            showError("This pin can be used as GPIO Only");
            return;
        }
    /* only alternate function = 0 is possible */
    case 0:
    case 2:
    case 5:
    case 6:
    case 11:
    case 12:
        alternativeMode = false;
        break;
    default:
        break;
    }
    LCR_SetGPIOConfig(ui->GPIO_Pin_comboBox->currentIndex(), ui->GPIO_EnAlt_checkBox->isChecked(), alternativeMode, ui->GPIO_PinDir_comboBox->currentIndex(),\
                      ui->GPIOoutType_comboBox->currentIndex(), ui->GPIO_PinState_comboBox->currentIndex());
}

void MainWindow::on_GPIOGetAlt_pushButton_clicked()
{
    bool enAltFunc, altFunc1, dirOutput, outTypeOpenDrain, state;

    if(LCR_GetGPIOConfig(ui->GPIO_Pin_comboBox->currentIndex(), &enAltFunc, &altFunc1, &dirOutput, &outTypeOpenDrain, &state) == 0)
    {
        ui->GPIO_EnAlt_checkBox->setChecked(enAltFunc);
        ui->GPIO_PinDir_comboBox->setCurrentIndex(dirOutput);
        ui->GPIOoutType_comboBox->setCurrentIndex(outTypeOpenDrain);
        ui->GPIO_PinState_comboBox->setCurrentIndex(state);
    }
}

void MainWindow::on_GpClk_spinBox_valueChanged(int arg1)
{
    char textStr[64];

    sprintf(textStr, "%2.3f Mhz", 96.0/arg1);
    ui->GpClk_Freq_label->setText(textStr);
}

void MainWindow::on_GpClkSet_pushButton_clicked()
{
    LCR_SetGeneralPurposeClockOutFreq(ui->GpClk_comboBox->currentIndex()+1, ui->GpClk_checkBox->isChecked(), ui->GpClk_spinBox->value());
}

void MainWindow::on_GpClkGet_pushButton_clicked()
{
    bool enabled;
    unsigned int divider;

    if(LCR_GetGeneralPurposeClockOutFreq(ui->GpClk_comboBox->currentIndex()+1, &enabled, &divider) == 0)
    {
        ui->GpClk_checkBox->setChecked(enabled);
        ui->GpClk_spinBox->setValue(divider);
    }
}

void MainWindow::on_browseInputBmp_clicked()
{
    QString startPath;
    QString fileName;

    if(patternFile.isFile())
        startPath = patternFile.absolutePath();
    else
        startPath = ptnImagePath;

    fileName = QFileDialog::getOpenFileName(this,
                                              QString("Select Image(s) for Pattern : "),
                                startPath,
                                "*.bmp");

    patternFile.setFile(fileName);
    ptnImagePath = patternFile.absolutePath();

    ui->inputBmpFileName->setText(patternFile.fileName());

    QImage img = QImage(fileName);
    if(img.width() != 912 || img.height() != 1140)
    {
        showError("Only images of resolution 912x1140 are supported");
        return;
    }
    QSize imgSize(ui->patternView->size());

    if(patternFile.isFile())
    {
        img.load(patternFile.absoluteFilePath());
    }

    ui->patternView->setPixmap(QPixmap::fromImage(img).scaled(imgSize));
}

void MainWindow::on_AddToOutFile_button_clicked()
{
    QFile file(patternFile.absoluteFilePath());
    QDataStream in(&file);
    unsigned char writeParam;

    writeParam = ui->fileBitPos_combobox->currentIndex() & 0x1F;
    writeParam |= ((ui->fileBitDepth_combobox->currentIndex() & 7) << 5);

    file.open(QIODevice::ReadOnly);
    BMP_ParseImage((BMP_DataFunc_t *)My_FileRead, &in, (BMP_PixelFunc_t *)My_ImgeDraw, &writeParam, ui->fileBitDepth_combobox->currentIndex()+1);
    file.close();

    emit on_fileSave_pushButton_clicked();

}

void MainWindow::on_fileSave_pushButton_clicked()
{
    BMP_Image_t splashImage;
    QFile outFile(outFileName);

    BMP_InitImage(&splashImage, PTN_WIDTH, PTN_HEIGHT, 8*BYTES_PER_PIXEL);
    if(outFile.open(QIODevice::ReadWrite))
    {
        BMP_StoreImage(&splashImage, (BMP_DataFunc_t *)My_FileWrite, &outFile, (BMP_PixelFunc_t *)My_ImgeGet, NULL);
        outFile.close();
    }
    else
    {
        showError("File Save Error");
        return;
    }

    QImage img(PTN_WIDTH, PTN_HEIGHT, QImage::Format_RGB32);
    QSize imgSize(ui->patternView_2->size());

    img.load(outFile.fileName());

    ui->patternView_2->setPixmap(QPixmap::fromImage(img).scaled(imgSize));
}

void MainWindow::on_clearOutFile_Button_clicked()
{
    memset(pImageBuffer, 0, PTN_WIDTH*PTN_HEIGHT*BYTES_PER_PIXEL);
    emit on_fileSave_pushButton_clicked();
}


void MainWindow::on_browseOutBmp_But_clicked()
{

    outFileName = QFileDialog::getSaveFileName(this,
                                              QString("Select File to Save the output bitmap : "),
                                outFileName,
                                "*.bmp");

    settings.setValue("outFileName", outFileName);
    ui->outBmpFileName->setText(outFileName);
}

void MainWindow::on_outBmpFileName_textEdited(const QString &arg1)
{
   settings.setValue("outFileName", arg1);
   outFileName = arg1;
}

void MainWindow::on_frame_splash_index_valueChanged(int index)
{
    FrameIdx = index;
}


void MainWindow::on_get_splash_timing_button_clicked()
{
    unsigned int pTimingData;
    unsigned char HWStatus, SysStatus, MainStatus;

    ui->splash_timing->clear();

    if(LCR_MeasureSplashLoadTiming(ui->splash_start_index->value(), 1) >= 0)
    {

        if(LCR_GetStatus(&HWStatus, &SysStatus, &MainStatus) != 0)
        {
            showError("Unable to read status");
            return;
        }

        if(LCR_ReadSplashLoadTiming(&pTimingData) >= 0)
        {
            ui->splash_timing->setText(numToStr(pTimingData/18667));
        }
    }
}
void MainWindow::on_splashImage_comboBox_currentIndexChanged(int index)
{
    if (added_SplashImages[index] == NULL)
    {
	    ui->dispSplashImage->clear();
	    return;
    }

    QFile imgFile(added_SplashImages[index]);
    QImage img(PTN_WIDTH, PTN_HEIGHT, QImage::Format_RGB32);
    QSize imgSize(ui->dispSplashImage->size());

    imgFile.open(QIODevice::ReadOnly);
    img.load(imgFile.fileName());

    ui->dispSplashImage->setPixmap(QPixmap::fromImage(img).scaled(imgSize));
    return;    
}

void MainWindow::on_addSplashImageButton_clicked()
{
	QString fileName;
       	static QString filePath;

	if (splashImageAddIndex == MAX_SPLASH_IMAGES)
	{
		showError("Only 64 images can be added \n");
		return;
	}

	if (filePath == NULL)
		filePath = ptnImagePath;
	fileName = QFileDialog::getOpenFileName(this,
                                QString("Select Splash Image"),
                                            filePath,
                                "*.bmp");
	if(!fileName.isEmpty())
    {
        QFileInfo patternFileInfo;
        patternFileInfo.setFile(fileName);
        ptnImagePath = patternFileInfo.absolutePath();

        QImage img = QImage(fileName);
        if(img.width() != 912 || img.height() != 1140)
        {
            showError("Only images of resolution 912x1140 are supported");
            return;
        }
        added_SplashImages[splashImageAddIndex] = fileName;
        filePath = fileName;
        if (splashImageAddIndex)
            ui->splashImage_comboBox->addItem(QString::number(splashImageAddIndex));
        ui->splashImage_comboBox->setCurrentIndex(splashImageAddIndex);
        emit on_splashImage_comboBox_currentIndexChanged(splashImageAddIndex);
        splashImageAddIndex++;

        ui->splashImageTotalCount->setText(displayStr_splashImageTotalCount + QString::number(++splashImageCount));
        ui->splashImageAddedCount->setText(displayStr_splashImageAddedCount + QString::number(++splashImageAdded));
    }
	return;
}

void MainWindow::on_removeSplashImagepushButton_clicked()
{
	int i;
	int index = ui->splashImage_comboBox->currentIndex();

	if (!splashImageAddIndex)
		return;

	added_SplashImages[index].clear();
	for(i = index; i < MAX_SPLASH_IMAGES - 1; i++)
	{
		if((added_SplashImages[i] == NULL) && (added_SplashImages[i + 1] == NULL))
			break;
		added_SplashImages[i] = added_SplashImages[i + 1];
	}

	emit on_splashImage_comboBox_currentIndexChanged(index);
	splashImageAddIndex--;
	if (splashImageAddIndex)
		ui->splashImage_comboBox->removeItem(splashImageAddIndex);	
	ui->splashImageTotalCount->setText(displayStr_splashImageTotalCount + QString::number(--splashImageCount));
	ui->splashImageRemovedCount->setText(displayStr_splashImageRemovedCount + QString::number(++splashImageRemoved));

	return;
}

void MainWindow::on_changeSplashImagepushButton_clicked()
{
	QString fileName;
       	static QString filePath;
	int index = ui->splashImage_comboBox->currentIndex();

	if (added_SplashImages[index] == NULL)
	{
		showError("Use Add Button to add patterns in sequence\n");
		return;
	}
	
	if (filePath == NULL)
		filePath = ptnImagePath;
	fileName = QFileDialog::getOpenFileName(this,
                                QString("Select Image"),
                                            filePath,
                                "*.bmp");

    if(!fileName.isEmpty())
    {
        QFileInfo patternFileInfo;
        patternFileInfo.setFile(fileName);
        ptnImagePath = patternFileInfo.absolutePath();

        QImage img = QImage(fileName);
        if(img.width() != 912 || img.height() != 1140)
        {
            showError("Only images of resolution 912x1140 are supported");
            return;
        }

        added_SplashImages[index] = fileName;
        filePath = fileName;
        emit on_splashImage_comboBox_currentIndexChanged(index);
    }
    return;
}

void MainWindow::on_buildNewFrmwImageButton_clicked()
{
	int i, count = 0;
	QString fileName;
	unsigned char *newFrmwImage;
    uint32 newFrmwSize;

	fileName = QFileDialog::getSaveFileName(this,
                                QString("Enter name of new Image to be built"),
                                            firmwarePath,
                                "*.img *.bin");

	if(fileName.isEmpty())
		return;

    QFileInfo firmwareFileInfo;
    firmwareFileInfo.setFile(fileName);
    firmwarePath = firmwareFileInfo.absolutePath();

	/* Check if .ini file is selected and if yes build it into the new firmware */
	if (!ui->iniFilelineEdit->text().isEmpty())
	{
		QFile iniFile(ui->iniFilelineEdit->text());
		if(!iniFile.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			showError("Unable to open .ini file");
			return;
		}
		QTextStream in(&iniFile);  

		while(!in.atEnd())
    		{
			QString firstIniToken;
			uint32 iniParams[128];
			int numIniParams, ret;

			QString line = in.readLine();
			if (!Frmw_ParseIniLines(line))
			{
				Frmw_GetCurrentIniLineParam(&firstIniToken, iniParams, &numIniParams);
				ret = Frmw_WriteApplConfigData(firstIniToken, iniParams, numIniParams);
				if (ret)
				{
					char errString[255];
					sprintf(errString, "wrong token %s or wrong parameters for the token %s\n", firstIniToken.toLocal8Bit().constData(), firstIniToken.toLocal8Bit().constData());
					showError(errString);
				}
    			}
		}
	}
	for(i = 0; i < MAX_SPLASH_IMAGES; i++)
	{
		if (added_SplashImages[i] != NULL)
			count++;
	}
	
	Frmw_SPLASH_InitBuffer(count);

	QFile logFile("Frmw-build.log");
	logFile.open(QIODevice::WriteOnly | QIODevice::Text);
	QTextStream out(&logFile);
    out << "Building Images from specified BMPs\n\n";
	for(i = 0; i < MAX_SPLASH_IMAGES; i++)
	{
		if (added_SplashImages[i] == NULL)
			continue;

		QFile imgFile(added_SplashImages[i]);
		unsigned char *pByteArray;
		uint8 compression;
		uint32 compSize;
		int fileLen, ret;
        char dbgStr[1024];
        QFileInfo imageFileInfo;
        imageFileInfo.setFile(imgFile.fileName());

		imgFile.open(QIODevice::ReadOnly);
		fileLen = imgFile.size();
		pByteArray = (unsigned char *)malloc(fileLen);
		
		if(pByteArray == NULL)
		{
			imgFile.close();
			showError("Memory alloc for file read failed");
			return;
       		}
		imgFile.read((char *)pByteArray, fileLen);
		imgFile.close();

		out << added_SplashImages[i] << "\n";
		out << "\t" << "Uncompressed Size = " << fileLen << " Compression type : ";
		
		if (added_SplashImages[i].contains("_nocomp.bmp"))
			compression = SPLASH_UNCOMPRESSED;
		else if (added_SplashImages[i].contains("_rle.bmp"))
			compression = SPLASH_RLE_COMPRESSION;
		else if (added_SplashImages[i].contains("_4line.bmp"))
			compression = SPLASH_4LINE_COMPRESSION;
		else
			compression = SPLASH_NOCOMP_SPECIFIED;

		ret = Frmw_SPLASH_AddSplash(pByteArray, &compression, &compSize);
		if (ret < 0)
        {
            switch(ret)
            {
            case ERROR_NOT_BMP_FILE:
                sprintf(dbgStr, "Error building firmware - %s not in BMP format", qPrintable(imageFileInfo.fileName()));
                break;
            case ERROR_NOT_24bit_BMP_FILE:
                sprintf(dbgStr, "Error building firmware - %s not in 24-bit format", qPrintable(imageFileInfo.fileName()));
                break;
            case ERROR_NO_MEM_FOR_MALLOC:
                sprintf(dbgStr, "Error building firmware with %s - Insufficient memory", qPrintable(imageFileInfo.fileName()));
                break;
            default:
                sprintf(dbgStr, "Error building firmware with %s - error code %d", qPrintable(imageFileInfo.fileName()), ret);
                break;
            }
            showError(dbgStr);
            return;
        }
 
		switch(compression)
		{
			case SPLASH_UNCOMPRESSED:
				out << "Uncompressed";
				break;
			case SPLASH_RLE_COMPRESSION:
				out << "RLE Compression";
				break;
			case SPLASH_4LINE_COMPRESSION:
				out << "4 Line Compression";
				break;
			default:
				break;
		}
		out << " Compressed Size = " << compSize << "\n\n";

		free(pByteArray);
	}

	logFile.close();
        Frmw_Get_NewFlashImage(&newFrmwImage, &newFrmwSize);

	QFile outFile(fileName);
        if(outFile.open(QIODevice::ReadWrite))
	{
        	if(outFile.write((char*)newFrmwImage, newFrmwSize) <= 0)
            		showError("Error in writing to output file\n");
		outFile.close();
	}
	else
		showError("Cannot Open output file for writing\n");

	ui->newFirmwareFile->setText(fileName);
	showError("Build Complete\n");
}

void MainWindow::on_SplashImageLoadButton_clicked()
{
	int i, count = 0, startSector=0, lastSectorToErase = 0, dataLen = 0, dataLen_full = 0, bytesSent = 0;
	unsigned char *newSplashBuffer;
	uint32 newSplashBufferSize, flashSkipSize;
	QFile flashParamFile(":/new/prefix1/Flash/FlashDeviceParameters.txt");
	unsigned short manID;
	unsigned long long devID;
	

	for(i = 0; i < MAX_SPLASH_IMAGES; i++)
	{
		if (added_SplashImages[i] != NULL)
			count++;
	}
	
	Frmw_SPLASH_InitBuffer(count);

    if(LCR_EnterProgrammingMode() < 0)
    	{
        	showError("Unable to enter Programming mode");
        	return;
    	}

	ui->Progressbar_action_label->setText("Waiting to enter programming mode");

	QTime waitEndTime = QTime::currentTime().addSecs(5);
	//If going from normal mode to bootloader mode, wait until connection closes; then wait again for reconnection
	while(ui->ConnectToolButton->isEnabled() == true)
	{
		QApplication::processEvents(); //Update the GUI timeout in case the target is already in bootloader mode.
		if(QTime::currentTime() > waitEndTime)
            		break;
    	}

	while(ui->ConnectToolButton->isEnabled() == false)
    	{
        	QApplication::processEvents(); //Update the GUI
    	}

    	usbPollTimer->stop();

	if(LCR_GetFlashManID(&manID) < 0)
    	{
        	showError("Unable to read Flash Manufacturer ID");
	        return;
    	}
    	if(LCR_GetFlashDevID(&devID) < 0)
    	{
        	showError("Unable to read Flash Device ID");
        	return;
    	}

	devID &= 0xFFFF;

	myFlashDevice.Mfg_ID = manID;
	myFlashDevice.Dev_ID = devID;

	if (!flashParamFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
        	showError("Unable to open FlashDeviceParameters.txt");
        	return;
    	}

	QTextStream in(&flashParamFile);
	bool found = false;
	while (!in.atEnd())
    	{
		QString line = in.readLine();
 		if(process_flash_params_line(line))
		{
	            found = true;
        	    break;
        	}
    	}

	if(found == false)
	{
		char displayStr[255];

        	sprintf(displayStr, "Unsupported Flash Device : Manufacturer ID = 0x%x & Device ID = 0x%llx", manID, devID);
	        showError(displayStr);
        	return;
    	}

	LCR_SetFlashType(myFlashDevice.Type);
	
	flashSkipSize = Frmw_GetSPlashFlashStartAddress() - FLASH_BASE_ADDRESS;
	startSector = GetSectorNum(flashSkipSize);

	if(flashSkipSize != myFlashDevice.SectorArr[startSector])
	{
        unsigned char *flashTableSector_Buffer = (unsigned char *) malloc(128 * 1024);
		
		startSector++;		
		Frmw_UpdateFlashTableSplashAddress(flashTableSector_Buffer, myFlashDevice.SectorArr[startSector]);
		ui->Progressbar_action_label->setText("Updating Flash Table");
		ui->Dnld_progressBar->setValue(0);
		
		LCR_SetFlashAddr(128 * 1024);
		LCR_FlashSectorErase();
		LCR_WaitForFlashReady();

		dataLen = 128 * 1024;
		dataLen_full = dataLen;

	        LCR_SetFlashAddr(128 * 1024);
	        LCR_SetDownloadSize(dataLen);
		
	       	while(dataLen > 0)
    		{
		        bytesSent = LCR_DownloadData(flashTableSector_Buffer+dataLen_full-dataLen, dataLen);

		        if(bytesSent < 0)
		        {
            			showError("Flash Table Data Download Failed");
		                free(flashTableSector_Buffer);
			        return;
        		}
/*        for(i=0; i<bytesSent; i++)
        {
            expectedChecksum += pByteArray[BLsize+dataLen_full-dataLen+i];
        } */

		        dataLen -= bytesSent;
		        ui->Dnld_progressBar->setValue(((dataLen_full-dataLen)*100/dataLen_full));
		        QApplication::processEvents(); //Update the GUI
    		}
	
		ui->Progressbar_action_label->setText("Flash Table Updated");
	        LCR_WaitForFlashReady();
		flashSkipSize = myFlashDevice.SectorArr[startSector] - FLASH_BASE_ADDRESS;		
	}
	
	lastSectorToErase = GetSectorNum(newSplashBufferSize + flashSkipSize);

	if((newSplashBufferSize + flashSkipSize) == myFlashDevice.SectorArr[lastSectorToErase]) //If perfectly aligned with last sector start addr, no need to erase last sector.
        	lastSectorToErase -= 1;

	
	ui->Dnld_progressBar->setValue(0);
    ui->Progressbar_action_label->setText("Erasing Flash Sectors to download images");

	for(i=startSector; i <= lastSectorToErase; i++)
    	{
        	LCR_SetFlashAddr(myFlashDevice.SectorArr[i]);
		LCR_FlashSectorErase();
		LCR_WaitForFlashReady();    //Wait for flash busy flag to go off
		ui->Dnld_progressBar->setValue(i*100/lastSectorToErase);
		QApplication::processEvents(); //Update the GUI
    	}

	ui->Progressbar_action_label->setText("Erasing Flash Sectors Complete");
	
	dataLen = dataLen_full = bytesSent = 0;

	for(i = 0; i < MAX_SPLASH_IMAGES; i++)
	{
		if (added_SplashImages[i] == NULL)
			continue;

		QFile imgFile(added_SplashImages[i]);
		unsigned char *pByteArray;
		uint8 compression;
		uint32 compSize;
		int fileLen, ret;

		imgFile.open(QIODevice::ReadOnly);
		fileLen = imgFile.size();
		pByteArray = (unsigned char *)malloc(fileLen);
		
		if(pByteArray == NULL)
		{
			imgFile.close();
			showError("Memory alloc for file read failed");
			return;
       		}
		imgFile.read((char *)pByteArray, fileLen);
		imgFile.close();
		if (added_SplashImages[i].contains("_nocomp.bmp"))
			compression = SPLASH_UNCOMPRESSED;
		else if (added_SplashImages[i].contains("_rle.bmp"))
			compression = SPLASH_RLE_COMPRESSION;
		else if (added_SplashImages[i].contains("_4line.bmp"))
			compression = SPLASH_4LINE_COMPRESSION;
		else
			compression = SPLASH_NOCOMP_SPECIFIED;

		ret = Frmw_SPLASH_AddSplash(pByteArray, &compression, &compSize);
		if (ret < 0)
            showError("Error in buiding flash image with specified images\n");
		free(pByteArray);
	}

	Frmw_Get_NewSplashBuffer(&newSplashBuffer, &newSplashBufferSize);
	
	dataLen = newSplashBufferSize;
	dataLen_full = dataLen;
	
        LCR_SetFlashAddr(myFlashDevice.SectorArr[startSector]);
        LCR_SetDownloadSize(dataLen);
	
	ui->Dnld_progressBar->setValue(0);	
    ui->Progressbar_action_label->setText("Downloading Images");

       	while(dataLen > 0)
	{
	        bytesSent = LCR_DownloadData(newSplashBuffer+dataLen_full-dataLen, dataLen);

	        if(bytesSent < 0)
	        {
                        showError("Image Data Download Failed");
			        return;
        	}
/*        for(i=0; i<bytesSent; i++)
        {
            expectedChecksum += pByteArray[BLsize+dataLen_full-dataLen+i];
        } */

		        dataLen -= bytesSent;
		        ui->Dnld_progressBar->setValue(((dataLen_full-dataLen)*100/dataLen_full));
		        QApplication::processEvents(); //Update the GUI
    		}
	
    ui->Progressbar_action_label->setText("Downloading Images Complete");
	LCR_WaitForFlashReady();
	
	LCR_ExitProgrammingMode();
	usbPollTimer->start();
}

void MainWindow::on_GPIO_EnAlt_checkBox_toggled(bool checked)
{
    ui->GPIO_PinDir_comboBox->setEnabled(!checked);
    ui->GPIO_PinState_comboBox->setEnabled(!checked);
    ui->GPIOoutType_comboBox->setEnabled(!checked);
}

void MainWindow::on_Export_pushButton_clicked()
{
    QString line;
    int slNo;
    if(ui->Seq_listWidget->count() == 0)
    {
        showError("Nothing to Export");
        return;
    }

    csvFileName = QFileDialog::getSaveFileName(this,
                                QString("Enter export file name"),
                                            csvFileName,
                                "*.csv");

    if(csvFileName.isEmpty())
    {
        return;
    }
    settings.setValue("csvFileName", csvFileName);
    QFile csvFile(csvFileName);

    if(csvFile.open(QIODevice::WriteOnly | QIODevice::Text) == 0)
    {
        showError("File Open Failed");
        return;
    }

    line = "SL NO.,BIT-DEPTH,PATTERN,LED,TRIG-IN,PAT-INVERT,INSERT-BLACK,TRIG-OUT,IMAGE/FRAME-INDEX";

    QTextStream out(&csvFile);
    out << line << "\n";

    slNo = 0;
    for(int i=0; i<ui->Seq_listWidget->count(); i++)
    {
        if(!ui->Seq_listWidget->item(i)->icon().isNull())
            continue;   //skip trigger items

        line = numToStr(slNo++);
        line.append(",");
        line.append(ui->Seq_listWidget->item(i)->data(Qt::UserRole+BitDepth).toString());
        line.append(",");
        line.append(ui->Seq_listWidget->item(i)->data(Qt::UserRole+PatNum).toString());
        line.append(",");
        switch(ui->Seq_listWidget->item(i)->data(Qt::UserRole+LEDSelect).toUInt())
        {
        case 1:
            line.append("RED,");
            break;
        case 2:
            line.append("GREEN,");
            break;
        case 3:
            line.append("YELLOW,");
            break;
        case 4:
            line.append("BLUE,");
            break;
        case 5:
            line.append("MAGENTA,");
            break;
        case 6:
            line.append("CYAN,");
            break;
        case 7:
            line.append("WHITE,");
            break;
        }
        switch(ui->Seq_listWidget->item(i)->data(Qt::UserRole+TrigType).toUInt())
        {
        case IntTrig:
            line.append("INTERNAL,");
            break;
        case ExtPos:
            line.append("EXTERNAL POS,");
            break;
        case ExtNeg:
            line.append("EXTERNAL NEG,");
            break;
        case NoTrig:
            line.append("CONTINUE,");
            break;
        }
        if(ui->Seq_listWidget->item(i)->data(Qt::UserRole+InvertPat).toUInt())
            line.append("YES,");
        else
            line.append("NO,");
        if(ui->Seq_listWidget->item(i)->data(Qt::UserRole+InsertBlack).toUInt())
            line.append("YES,");
        else
            line.append("NO,");
        if(ui->Seq_listWidget->item(i)->data(Qt::UserRole+trigOutPrev).toUInt())
            line.append("PREV,");
        else
            line.append("NEW,");

        line.append(ui->Seq_listWidget->item(i)->data(Qt::UserRole+frameIndex).toString());

        out << line << "\n";
    }

    if(numExtraSplashLutEntries)
    {
	    line = "129";
	    line.append(",");
	    for (int i = 0; i < numExtraSplashLutEntries; i++)
	    {
		    line.append(numToStr(extraSplashLutEntries[i]));
		    line.append(",");
	    }
	    out << line << "\n";
    }
    csvFile.close();
}

void MainWindow::on_Import_pushButton_clicked()
{
    int trigger_type;
    char errorStr[256];
    int pat_num, bitDepth, Led_Select, frame_index;
    bool invertPat, insertBlack, bufSwap, TrigOutPrev;
    int maxPatNum;
    int last_frame_index = -1;

    csvFileName = QFileDialog::getOpenFileName(this,
                                              QString("Select CSV file for import : "),
                                csvFileName,
                                "*.csv");
    if(csvFileName.isEmpty())
    {
        return;
    }
    settings.setValue("csvFileName", csvFileName);
    QFile csvFile(csvFileName);

    if(csvFile.open(QIODevice::ReadOnly | QIODevice::Text) == 0)
    {
        showError("File Open Failed");
        return;
    }

    emit on_clearAllButton_clicked();

    QTextStream in(&csvFile);
    int slNo=-1;
    while (!in.atEnd())
    {
        QString line = in.readLine();
        if(slNo++ == -1)
            continue;

	/* Special case where if SL number is 129, the entries correspond to extra Splash LUT entries */
	if (line.section(',', 0,0).trimmed().toInt() == 129)
	{
		bool ok = true;
		int i = 1;
		while (1)
		{
			int splashNum = line.section(',', i,i).trimmed().toInt(&ok);
			if (!ok)
				break;
			extraSplashLutEntries[numExtraSplashLutEntries++] = splashNum;
			i++;
		}
		continue;
	}

        if(line.section(',', 4,4).trimmed().compare("INTERNAL", Qt::CaseInsensitive) == 0)
            trigger_type = IntTrig;
        else if(line.section(',', 4,4).trimmed().compare("EXTERNAL POS", Qt::CaseInsensitive) == 0)
            trigger_type = ExtPos;
        else if(line.section(',', 4,4).trimmed().compare("EXTERNAL NEG", Qt::CaseInsensitive) == 0)
            trigger_type = ExtNeg;
        else if(line.section(',', 4,4).trimmed().compare("CONTINUE", Qt::CaseInsensitive) == 0)
            trigger_type = NoTrig;
        else
        {
            sprintf(errorStr, "Unknown trigger type in CSV File line# %d", slNo);
            showError(errorStr);
            return;
        }
        insertTriggerItem(trigger_type);

        bitDepth = line.section(',', 1, 1).trimmed().toUInt();
        if(bitDepth < 1 || bitDepth > 8)
        {
            sprintf(errorStr, "Invalid bit-depth in CSV File line# %d", slNo);
            showError(errorStr);
            return;
        }
        pat_num = line.section(',', 2, 2).trimmed().toUInt();

        if(bitDepth == 1)
            maxPatNum =24;
        else if(bitDepth == 5)
            maxPatNum = 3;
        else if(bitDepth == 7)
            maxPatNum = 2;
        else
            maxPatNum = 24/bitDepth - 1;
        if(pat_num > maxPatNum)
        {
            sprintf(errorStr, "Invalid pattern-number in CSV File line# %d", slNo);
            showError(errorStr);
            return;
        }

        if(line.section(',', 3,3).trimmed().compare("RED", Qt::CaseInsensitive) == 0)
            Led_Select=1;
        else if(line.section(',', 3,3).trimmed().compare("GREEN", Qt::CaseInsensitive) == 0)
            Led_Select=2;
        else if(line.section(',', 3,3).trimmed().compare("YELLOW", Qt::CaseInsensitive) == 0)
            Led_Select=3;
        else if(line.section(',', 3,3).trimmed().compare("BLUE", Qt::CaseInsensitive) == 0)
            Led_Select=4;
        else if(line.section(',', 3,3).trimmed().compare("MAGENTA", Qt::CaseInsensitive) == 0)
            Led_Select=5;
        else if(line.section(',', 3,3).trimmed().compare("CYAN", Qt::CaseInsensitive) == 0)
            Led_Select=6;
        else if(line.section(',', 3,3).trimmed().compare("WHITE", Qt::CaseInsensitive) == 0)
            Led_Select=7;
        else
        {
            sprintf(errorStr, "Invalid LED specified in CSV File line# %d", slNo);
            showError(errorStr);
            return;
        }
        if(line.section(',', 5,5).trimmed().compare("YES", Qt::CaseInsensitive) == 0)
            invertPat = true;
        else
            invertPat = false;
        if(line.section(',', 6,6).trimmed().compare("YES", Qt::CaseInsensitive) == 0)
            insertBlack = true;
        else
            insertBlack = false;
        if(line.section(',', 7,7).trimmed().compare("NEW", Qt::CaseInsensitive) == 0)
            TrigOutPrev = false;
        else
            TrigOutPrev = true;

        frame_index = line.section(',', 8,8).trimmed().toUInt();

        if(frame_index != last_frame_index)
            bufSwap= true;
        else
            bufSwap=false;

        last_frame_index = frame_index;

        if(pat_num == 24)
            ui->Seq_listWidget->addItem(GenerateItemText(frame_index, bitDepth, 0, 0));
        else
            ui->Seq_listWidget->addItem(GenerateItemText(frame_index, bitDepth, pat_num*bitDepth, (pat_num+1)*bitDepth-1));
        int seqListLength = ui->Seq_listWidget->count();

        QColor bgColor = GetColorFromIndex(Led_Select);
        if(pat_num == 24)
            bgColor = Qt::black;
        ui->Seq_listWidget->item(seqListLength-1)->setBackgroundColor(bgColor);
        updateSeqItemData(trigger_type, pat_num, bitDepth, Led_Select, frame_index, invertPat, insertBlack, bufSwap, TrigOutPrev);

    }
}

void MainWindow::applyIniParam(QString token, uint32 *params, int numParams)
{
    static unsigned char splashLut[64];
    static int numSplashLutEntries = 0;
    int splashIndex = 0, frameIndex;
    char dispStr[255];
    
    switch(iniGUITokens.indexOf(token))
    {
	    case 0:				//DEFAULT.DISPMODE
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.DISPMODE\n");

		    if (params[0])
			    ui->Radiobutton_SLmode->click();
		    else
			    ui->RadioButton_videoMode->click();
		    break;

	    case 1:				//DEFAULT.SHORT_FLIP
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.DISPMODE\n");
		    
		    if (params[0])
			    ui->ShortAxisFlipCheckBox->setChecked(true);
		    else
			    ui->ShortAxisFlipCheckBox->setChecked(false);

		    break;
	    case 2:				//DEFAULT.LONG_FLIP
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.DISPMODE\n");
		    
		    if (params[0])
			    ui->LongAxisFlipCheckBox->setChecked(true);
		    else
			    ui->LongAxisFlipCheckBox->setChecked(false);			    

		    break;
	    case 3:				//DEFAULT.TRIG_OUT_1.POL
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.DISPMODE\n");

		    if (params[0])
			    ui->InvertTrig1Out_checkBox->setChecked(true);
		    else
			    ui->InvertTrig1Out_checkBox->setChecked(false);
		    break;
	    case 4:				//DEFAULT.TRIG_OUT_1.RDELAY
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.DISPMODE\n");

		    ui->Trig1Out_R_spinBox->setValue(params[0]);
		    break;
	    case 5:				//DEFAULT.TRIG_OUT_1.FDELAY
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.DISPMODE\n");

		    ui->Trig1Out_F_spinBox->setValue(params[0]);
		    break;
	    case 6:				//DEFAULT.TRIG_OUT_2.POL
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.DISPMODE\n");

		    if (params[0])
			    ui->InvertTrig2Out_checkBox->setChecked(true);
		    else
			    ui->InvertTrig2Out_checkBox->setChecked(false);
		    break;
	    case 7:				//DEFAULT.TRIG_OUT_2.WIDTH
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.DISPMODE\n");

		    ui->Trig2Out_R_spinBox->setValue(params[0]);
		    break;
	    case 8:				//DEFAULT.TRIG_IN_1.DELAY
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.DISPMODE\n");

		    ui->TrigIn1_spinBox->setValue(params[0]);
		    break;
	    case 9:				//DEFAULT.RED_STROBE.RDELAY
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.DISPMODE\n");

		    ui->Red_R_spinBox->setValue(params[0]);
		    break;
	    case 10:				//DEFAULT.RED_STROBE.FDELAY
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.DISPMODE\n");

		    ui->Red_F_spinBox->setValue(params[0]);
		    break;
	    case 11:				//DEFAULT.GRN_STROBE.RDELAY
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.GRN_STROBE.RDELAY\n");

		    ui->Green_R_spinBox->setValue(params[0]);
		    break;
	    case 12:				//DEFAULT.GRN_STROBE.FDELAY
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.GRN_STROBE.FDELAY\n");

		    ui->Green_F_spinBox->setValue(params[0]);
		    break;
	    case 13:				//DEFAULT.BLU_STROBE.RDELAY
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.BLU_STROBE.RDELAY\n");

		    ui->Blue_R_spinBox->setValue(params[0]);
		    break;
	    case 14:				//DEFAULT.BLU_STROBE.FDELAY
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.BLU_STROBE.FDELAY\n");

		    ui->Blue_F_spinBox->setValue(params[0]);
		    break;
	    case 15:				//DEFAULT.INVERTDATA
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.INVERTDATA\n");
		    if (params[0])
			    ui->InvertData_checkBox->setChecked(true);
		    else
			    ui->InvertData_checkBox->setChecked(false);
		    break;
	    case 16:				//DEFAULT.LEDCURRENT_RED
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.LEDCURRENT_RED\n");

		    ui->RedLEDCurrent->setText(QString::number(255 - params[0]));
		    break;
	    case 17:				//DEFAULT.LEDCURRENT_GRN
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.LEDCURRENT_GRN\n");

		    ui->GreenLEDCurrent->setText(QString::number(255 - params[0]));
		    
		    break;
	    case 18:				//DEFAULT.LEDCURRENT_BLU
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.LEDCURRENT_BLU\n");

		    ui->BlueLEDCurrent->setText(QString::number(255 - params[0]));			    
		    break;
	    case 19:				//DEFAULT.PATTERNCONFIG.PAT_EXPOSURE
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.PATTERNCONFIG.PAT_EXPOSURE\n");
		    ui->patExp_lineEdit->setText(QString::number(params[0]));

		    break;
	    case 20:				//DEFAULT.PATTERNCONFIG.PAT_PERIOD
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.PATTERNCONFIG.PAT_PERIOD\n");
		    ui->trigPeriod_lineEdit->setText(QString::number(params[0])); 
		    break;
	    case 21:				//DEFAULT.PATTERNCONFIG.PAT_MODE
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.PATTERNCONFIG.PAT_MODE\n");
		    if (params[0] == 0x3)
                ui->patFrom_flash->setChecked(true);
		    else if (params[0] == 0x0)
                ui->patFrom_video->setChecked(true);
		    else
			    showError("Wrong value as argument for DEFAULT.PATTERNCONFIG.PAT_MODE");
		    break;
	    case 22:				//DEFAULT.PATTERNCONFIG.TRIG_MODE
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.PATTERNCONFIG.TRIG_MODE\n");
		    if (params[0])
			    ui->IntExtTrigMode_But->setChecked(true);
		    else
			    ui->VsyncTrigMode_But->setChecked(true);
		    break;
	    case 23:				//DEFAULT.PATTERNCONFIG.PAT_REPEAT
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.PATTERNCONFIG.PAT_REPEAT\n");
		    if (params[0])
			    ui->patDispMode_radioButton_1->setChecked(true);
		    else
			    ui->patDispMode_radioButton->setChecked(false);
		    break;
	    case 24:				//DEFAULT.SPLASHLUT
			for(int i = 0; i < numParams; i++)
				splashLut[i] = params[i];
			numSplashLutEntries = numParams;
			break;
	    case 25:				//DEFAULT.SEQPATLUT
			emit on_clearAllButton_clicked();

			for(int i = 0; i < numParams; i++)
			{
				unsigned char patNum, bitDepth, trigger_type, maxPatNum, ledSelect;
				bool invertPat, insertBlack, bufSwap, trigOutPrev;

				trigger_type = params[i] & 0x3;
			        insertTriggerItem(trigger_type);
				
				patNum = (params[i] >> 2) & 0x3F;
				bitDepth = (params[i] >> 8) & 0xF;

				if(bitDepth < 1 || bitDepth > 8)
			        {
			            sprintf(dispStr, "Invalid bit-depth in PAT LUT entry%d ", i);
			            showError(dispStr);
			            continue;
				}

			        if(bitDepth == 1)
			            maxPatNum =24;
			        else if(bitDepth == 5)
			            maxPatNum = 3;
			        else if(bitDepth == 7)
			            maxPatNum = 2;
			        else
			            maxPatNum = 24/bitDepth - 1;
			        if(patNum > maxPatNum)
				{
			            sprintf(dispStr, "Invalid pattern-number in PAT LUT entry%d ", i);
			            showError(dispStr);
				    continue;
			        }

				ledSelect = (params[i] >> 12) & 0x7;

				if ((params[i] & 0x00010000))
					invertPat = true;
				else
					invertPat = false;

				if((params[i] & 0x00020000))
					insertBlack = true;
				else
					insertBlack = false;

				if((params[i] & 0x00040000) || (!i))
				{
					if (splashIndex >= numSplashLutEntries)
					{
						showError("Bad .ini! SplashLUT entries do not match the number of buffer swaps in PAT LUT");
						continue;
					}
					else
					{
						bufSwap = true;
						frameIndex = splashLut[splashIndex++];
					}
				}
				else
					bufSwap = false;

				if((params[i] & 0x00080000))
					trigOutPrev = true;
				else
					trigOutPrev = false;

			        if(patNum == 24)
			            ui->Seq_listWidget->addItem(GenerateItemText(frameIndex, bitDepth, 0, 0));
			        else
			            ui->Seq_listWidget->addItem(GenerateItemText(frameIndex, bitDepth, patNum*bitDepth, (patNum+1)*bitDepth-1));
			        int seqListLength = ui->Seq_listWidget->count();

			        QColor bgColor = GetColorFromIndex(ledSelect);
			        if(patNum == 24)
			            bgColor = Qt::black;
			        ui->Seq_listWidget->item(seqListLength-1)->setBackgroundColor(bgColor);
			        updateSeqItemData(trigger_type, patNum, bitDepth, ledSelect, frameIndex, invertPat, insertBlack, bufSwap, trigOutPrev);
			}
			if (numSplashLutEntries > splashIndex)
			{
				numExtraSplashLutEntries = numSplashLutEntries - splashIndex;
				for (int i = 0; i < numExtraSplashLutEntries; i++)
					extraSplashLutEntries[i] = splashLut[splashIndex++];
			}
			break;
	    case 26:				//DEFAULT.PORTCONFIG.port
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.PORTCONFIG.port\n");
			ui->InputSourceList->setCurrentIndex(params[0]);
			break;
	    case 27:				//DEFAULT.PORTCONFIG.bpp
		    if(numParams > 1)				
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.PORTCONFIG.bpp\n");
		    ui->InputSourceOptionList->setCurrentIndex(params[0]);
		    break;
	    case 28:				//DEFAULT.PORTCONFIG.pix_fmt
		    if(numParams > 1)				
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.PORTCONFIG.pix_fmt\n");
		    if((params[0] > 0) && ((ui->InputSourceList->currentIndex() == 1) || (ui->InputSourceList->currentIndex() == 3)))
		    {
			    showError("Wrong pixel format in the .ini file for the chosen port\n");
			    break;
		    }
		    if(ui->InputSourceList->currentIndex() == 2)
		    {
			    if ((params[0] == 1) || (params[0] > 2))
			    {
				    showError("Wrong pixel format in the .ini file for the chosen port\n");
				    break;
			    }
			    if (params[0] == 2)
				    params[0] = 1;
		    }
		    if((ui->InputSourceList->currentIndex() == 0) && (params[0] > 2))
		    {
			    showError("Wrong pixel format in the .ini file for the chosen port\n");
			    break;
		    }
		    ui->PixelFormatList->setCurrentIndex(params[0]);
		    break;
	    case 29:				//DEFAULT.PORTCONFIG.port_clk
		    if(numParams > 1)				
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.PORTCONFIG.port_clk\n");
		    ui->PortClockList->setCurrentIndex(params[0]);
		    break;
	    case 30:				//DEFAULT.PORTCONFIG.abc_mux
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.PORTCONFIG.abc_mux\n");
		    ui->SwapSelectList->setCurrentIndex(params[0]);
		    break;
	    case 31:				//DEFAULT.PORTCONFIG.pix_mode
		    if(numParams > 1)				
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.PORTCONFIG.pix_mode\n");
		    ui->FPD_PixMode_spinBox->setValue(params[0]);
		    break;
	    case 32:				//DEFAULT.PORTCONFIG.swap_pol
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.PORTCONFIG.swap_pol\n");
		    if(params[0])
			    ui->FPD_Inv_Pol_checkBox->setChecked(true);
		    else
			    ui->FPD_Inv_Pol_checkBox->setChecked(false);
		    break;
	    case 33:				//DEFAULT.PORTCONFIG.fld_sel
		    if(numParams > 1)
			    showError("Wrong number of parameters in the chosen .ini file for DEFAULT.PORTCONFIG.fld_sel\n");
		    ui->FPDFieldSelectList->setCurrentIndex(params[0]);
		    break;
	    default:
		    break;
    }	
}

void MainWindow::on_selectIniFileButton_clicked()
{
    QString fileName;
    fileName = QFileDialog::getOpenFileName(this,
                                QString("Select .ini file"),
                                            firmwarePath,
                                tr("ini files(*.ini)"));
    if(fileName.isEmpty())
	    return;

    QFileInfo firmwareFileInfo;
    firmwareFileInfo.setFile(fileName);
    firmwarePath = firmwareFileInfo.absolutePath();

    ui->iniFilelineEdit->setText(fileName);
}

void MainWindow::on_clearIniFileButton_clicked()
{
    ui->iniFilelineEdit->clear();	
}

void MainWindow::on_applySolutionButton_clicked()
{
    QString fileName;
    bool SLmode=0;

    fileName = QFileDialog::getOpenFileName(this,
                                QString("Select .ini file"),
                                            firmwarePath,
                                tr("ini files(*.ini)"));
    if(fileName.isEmpty())
	    return;

    QFileInfo firmwareFileInfo;
    firmwareFileInfo.setFile(fileName);
    firmwarePath = firmwareFileInfo.absolutePath();

    QFile iniFile(fileName);
    if(!iniFile.open(QIODevice::ReadWrite | QIODevice::Text))
    {
	showError("Unable to open .ini file");
	return;
    }

    QTextStream in(&iniFile);  

    while(!in.atEnd())
    {
	    QString firstIniToken;
	    uint32 iniParams[128];
	    int numIniParams;

	    QString line = in.readLine();
	    if (Frmw_ParseIniLines(line))
		    continue;

	    Frmw_GetCurrentIniLineParam(&firstIniToken, iniParams, &numIniParams);
	    applyIniParam(firstIniToken, iniParams, numIniParams);
    }

    emit on_SetFlipPushButton_clicked();

    LCR_GetMode(&SLmode);

    LCR_SetMode(0x1);
    emit on_TrigOutSet_pushButton_clicked();
    emit on_SetLEDpushButton_2_clicked();
    LCR_SetMode(0x0);
    emit on_SetLEDpushButton_clicked();

    LCR_SetMode(SLmode);

    emit on_SourceSetBut_clicked();
    emit on_PortClockSetBut_clicked();
    emit on_PixelFormatSetBut_clicked();
    emit on_SetSwapBut_clicked();
    emit on_FPDModeSetBut_clicked();

    iniFile.close();
}

void MainWindow::on_saveIniSignalReceived(int *iniParamsIsDefault)
{
	QString fileName;
	int nr_guiiniTokens = 0;

	fileName = QFileDialog::getSaveFileName(this,
                         QString("Enter name of the .ini file"),
                                 firmwarePath,
                                tr("ini files(*.ini)"));

	if(fileName.isEmpty())
	    return;

    QFileInfo firmwareFileInfo;
    firmwareFileInfo.setFile(fileName);
    firmwarePath = firmwareFileInfo.absolutePath();

	QFile iniFile(fileName);
	if(!iniFile.open(QIODevice::WriteOnly | QIODevice::Text))
    	{
		showError("Unable to open .ini file");
		return;
    	}
	
	QTextStream out(&iniFile);

	for (int i = 0; i < NR_INI_TOKENS; i++)
	{
		if ((i == DEFAULT_PATTERNCONFIG_NUM_LUT_ENTRIES) || (i == DEFAULT_PATTERNCONFIG_NUM_PATTERNS) || (i == DEFAULT_PATTERNCONFIG_NUM_SPLASH))
			continue;

		if((iniParam_Info[i].is_gui_editable) && (!iniParamsIsDefault[nr_guiiniTokens++]))
		{
			if (i == DEFAULT_SPLASHLUT)
				out << iniParam_Info[DEFAULT_PATTERNCONFIG_NUM_SPLASH].token << "        " << "0x" <<
					iniParam_Info[DEFAULT_PATTERNCONFIG_NUM_SPLASH].gui_defined_param[0] << " ;\n";

			if (i == DEFAULT_SEQPATLUT)
			{
				out << iniParam_Info[DEFAULT_PATTERNCONFIG_NUM_LUT_ENTRIES].token << "        " << "0x" <<
					iniParam_Info[DEFAULT_PATTERNCONFIG_NUM_LUT_ENTRIES].gui_defined_param[0] << " ;\n";

				out << iniParam_Info[DEFAULT_PATTERNCONFIG_NUM_PATTERNS].token << "        " << "0x" <<
					iniParam_Info[DEFAULT_PATTERNCONFIG_NUM_PATTERNS].gui_defined_param[0] << " ;\n";				
			}

			out << iniParam_Info[i].token << "        ";
			for (int j = 0; j < iniParam_Info[i].nr_user_defined_params; j++)
				out << "0x" << iniParam_Info[i].gui_defined_param[j] << " ";
		}
		else
		{
			if (i == DEFAULT_SPLASHLUT)
				out << iniParam_Info[DEFAULT_PATTERNCONFIG_NUM_SPLASH].token << "        " << "0x" <<
					iniParam_Info[DEFAULT_PATTERNCONFIG_NUM_SPLASH].default_param[0] << " ;\n";

			if (i == DEFAULT_SEQPATLUT)
			{
				out << iniParam_Info[DEFAULT_PATTERNCONFIG_NUM_LUT_ENTRIES].token << "        " << "0x" <<
					iniParam_Info[DEFAULT_PATTERNCONFIG_NUM_LUT_ENTRIES].default_param[0] << " ;\n";

				out << iniParam_Info[DEFAULT_PATTERNCONFIG_NUM_PATTERNS].token << "        " << "0x" <<
					iniParam_Info[DEFAULT_PATTERNCONFIG_NUM_PATTERNS].default_param[0] << " ;\n";				
			}
			
			out << iniParam_Info[i].token << "        ";
			for (int j = 0; j < iniParam_Info[i].nr_default_params; j++)
				out << "0x" << hex << iniParam_Info[i].default_param[j] << " ";
		}
		out << ";\n";		
	}
	iniFile.close();
}

void MainWindow::on_saveSolutionButton_clicked()
{
	int i, num_splashLut = 0, num_patLut = 0;

	iniParam_Info[DEFAULT_DISPMODE].gui_defined_param[0] = ui->Radiobutton_SLmode->isChecked();
	iniParam_Info[DEFAULT_SHORT_FLIP].gui_defined_param[0] = ui->ShortAxisFlipCheckBox->isChecked();
	iniParam_Info[DEFAULT_LONG_FLIP].gui_defined_param[0] = ui->LongAxisFlipCheckBox->isChecked();
	iniParam_Info[DEFAULT_TRIG_OUT_1_POL].gui_defined_param[0] = ui->InvertTrig1Out_checkBox->isChecked();
	iniParam_Info[DEFAULT_TRIG_OUT_1_RDELAY].gui_defined_param[0] = ui->Trig1Out_R_spinBox->value();
	iniParam_Info[DEFAULT_TRIG_OUT_1_FDELAY].gui_defined_param[0] = ui->Trig1Out_F_spinBox->value();
	iniParam_Info[DEFAULT_TRIG_OUT_2_POL].gui_defined_param[0] = ui->InvertTrig2Out_checkBox->isChecked();
	iniParam_Info[DEFAULT_TRIG_OUT_2_WIDTH].gui_defined_param[0] = ui->Trig2Out_R_spinBox->value();
	iniParam_Info[DEFAULT_TRIG_IN_1_DELAY].gui_defined_param[0] = ui->TrigIn1_spinBox->value();
	iniParam_Info[DEFAULT_RED_STROBE_RDELAY].gui_defined_param[0] = ui->Red_R_spinBox->value();
	iniParam_Info[DEFAULT_RED_STROBE_FDELAY].gui_defined_param[0] = ui->Red_F_spinBox->value();
	iniParam_Info[DEFAULT_GRN_STROBE_RDELAY].gui_defined_param[0] = ui->Green_R_spinBox->value();
	iniParam_Info[DEFAULT_GRN_STROBE_FDELAY].gui_defined_param[0] = ui->Green_F_spinBox->value();
	iniParam_Info[DEFAULT_BLU_STROBE_RDELAY].gui_defined_param[0] = ui->Blue_R_spinBox->value();
	iniParam_Info[DEFAULT_BLU_STROBE_FDELAY].gui_defined_param[0] = ui->Blue_F_spinBox->value();
	iniParam_Info[DEFAULT_INVERTDATA].gui_defined_param[0] = ui->InvertData_checkBox->isChecked();
	iniParam_Info[DEFAULT_LEDCURRENT_RED].gui_defined_param[0] = 255-strToNum(ui->RedLEDCurrent->text());
	iniParam_Info[DEFAULT_LEDCURRENT_GRN].gui_defined_param[0] = 255-strToNum(ui->GreenLEDCurrent->text());
	iniParam_Info[DEFAULT_LEDCURRENT_BLU].gui_defined_param[0] = 255-strToNum(ui->BlueLEDCurrent->text());
	iniParam_Info[DEFAULT_PATTERNCONFIG_PAT_EXPOSURE].gui_defined_param[0] = ui->patExp_lineEdit->text().toUInt();
	iniParam_Info[DEFAULT_PATTERNCONFIG_PAT_PERIOD].gui_defined_param[0] = ui->trigPeriod_lineEdit->text().toUInt();
    if (ui->patFrom_flash->isChecked())
		iniParam_Info[DEFAULT_PATTERNCONFIG_PAT_MODE].gui_defined_param[0] = 0x3;
	else
		iniParam_Info[DEFAULT_PATTERNCONFIG_PAT_MODE].gui_defined_param[0] = 0x0;
	iniParam_Info[DEFAULT_PATTERNCONFIG_TRIG_MODE].gui_defined_param[0] = ui->IntExtTrigMode_But->isChecked();
	iniParam_Info[DEFAULT_PATTERNCONFIG_PAT_REPEAT].gui_defined_param[0] = ui->patDispMode_radioButton_1->isChecked();

    for (i = 0; i < ui->Seq_listWidget->count(); i++)
	{
		unsigned int patLutEntry = 0;

        if(!ui->Seq_listWidget->item(i)->icon().isNull()) //Selected item is a trigger item
            continue;

		patLutEntry = ui->Seq_listWidget->item(i)->data(Qt::UserRole+TrigType).toInt() & 3;
		patLutEntry |= ((ui->Seq_listWidget->item(i)->data(Qt::UserRole+PatNum).toInt() & 0x3F) << 2);
		patLutEntry |= ((ui->Seq_listWidget->item(i)->data(Qt::UserRole+BitDepth).toInt() & 0xF) << 8);
		patLutEntry |= ((ui->Seq_listWidget->item(i)->data(Qt::UserRole+LEDSelect).toInt() & 0x7) << 12);
		if(ui->Seq_listWidget->item(i)->data(Qt::UserRole+InvertPat).toBool())
			patLutEntry |= BIT16;
		if(ui->Seq_listWidget->item(i)->data(Qt::UserRole+InsertBlack).toBool())
			patLutEntry |= BIT17;
		if(ui->Seq_listWidget->item(i)->data(Qt::UserRole+BufSwap).toBool())
			patLutEntry |= BIT18;
		if(ui->Seq_listWidget->item(i)->data(Qt::UserRole+trigOutPrev).toBool())
			patLutEntry |= BIT19;

		iniParam_Info[DEFAULT_SEQPATLUT].gui_defined_param[num_patLut++] = patLutEntry;

            	if((ui->Seq_listWidget->item(i)->data(Qt::UserRole+BufSwap).toBool()) || (i == 1))
			iniParam_Info[DEFAULT_SPLASHLUT].gui_defined_param[num_splashLut++] = ui->Seq_listWidget->item(i)->data(Qt::UserRole+frameIndex).toInt();
	}

	if(numExtraSplashLutEntries)
		for (i = 0; i < numExtraSplashLutEntries; i++)
			iniParam_Info[DEFAULT_SPLASHLUT].gui_defined_param[num_splashLut++] = extraSplashLutEntries[i];
	
	iniParam_Info[DEFAULT_SPLASHLUT].nr_user_defined_params = num_splashLut;
	iniParam_Info[DEFAULT_SEQPATLUT].nr_user_defined_params = num_patLut;

	iniParam_Info[DEFAULT_PATTERNCONFIG_NUM_LUT_ENTRIES].gui_defined_param[0] = num_patLut - 1;
	iniParam_Info[DEFAULT_PATTERNCONFIG_NUM_PATTERNS].gui_defined_param[0] = num_patLut - 1;
	iniParam_Info[DEFAULT_PATTERNCONFIG_NUM_SPLASH].gui_defined_param[0] = num_splashLut - 1;
	iniParam_Info[DEFAULT_PORTCONFIG_PORT].gui_defined_param[0] = ui->InputSourceList->currentIndex();
	iniParam_Info[DEFAULT_PORTCONFIG_BPP].gui_defined_param[0] = ui->InputSourceOptionList->currentIndex();

	if((ui->InputSourceList->currentIndex() == 2) && (ui->PixelFormatList->currentIndex() == 1))
		iniParam_Info[DEFAULT_PORTCONFIG_PIX_FMT].gui_defined_param[0] = 2;
	else
		iniParam_Info[DEFAULT_PORTCONFIG_PIX_FMT].gui_defined_param[0] = ui->PixelFormatList->currentIndex();

	iniParam_Info[DEFAULT_PORTCONFIG_PORT_CLK].gui_defined_param[0] = ui->PortClockList->currentIndex();
	iniParam_Info[DEFAULT_PORTCONFIG_ABC_MUX].gui_defined_param[0] = ui->SwapSelectList->currentIndex();
	iniParam_Info[DEFAULT_PORTCONFIG_PIX_MODE].gui_defined_param[0] =  ui->FPD_PixMode_spinBox->value();
	iniParam_Info[DEFAULT_PORTCONFIG_SWAP_POL].gui_defined_param[0] = ui->FPD_Inv_Pol_checkBox->isChecked();
	iniParam_Info[DEFAULT_PORTCONFIG_FLD_SEL].gui_defined_param[0] = ui->FPDFieldSelectList->currentIndex();

	iniSaveWindow = new IniSaveWindow();

	QObject::connect(iniSaveWindow, SIGNAL(saveIniButtonClicked(int *)),
			this, SLOT(on_saveIniSignalReceived(int *)));
	
	iniSaveWindow->show();
	return;
}

void MainWindow::on_applyDefaultSolutionButton_clicked()
{
	bool SLmode=0;

	for (int i = 0; i < NR_INI_TOKENS; i++)
		applyIniParam(iniParam_Info[i].token, iniParam_Info[i].default_param, iniParam_Info[i].nr_default_params);

	emit on_SetFlipPushButton_clicked();

	LCR_GetMode(&SLmode);

	LCR_SetMode(0x1);
	emit on_TrigOutSet_pushButton_clicked();
	emit on_SetLEDpushButton_2_clicked();
	LCR_SetMode(0x0);
	emit on_SetLEDpushButton_clicked();

	LCR_SetMode(SLmode);

	emit on_SourceSetBut_clicked();
	emit on_PortClockSetBut_clicked();
	emit on_PixelFormatSetBut_clicked();
	emit on_SetSwapBut_clicked();
	emit on_FPDModeSetBut_clicked();	
}


void MainWindow::on_patFrom_flash_toggled(bool checked)
{
    ui->frame_splash_index->setValue(0);
    patFromSplash = checked;
    updateFrameTitle();

    LCR_SetPatternDisplayMode(!checked);

    if(!checked)
    {
        ui->label_Seq_Timing_SplashVSysnc->setText("Vsync Period (us)");
        ui->framePer_lineEdit->setText(numToStr(16667));
        ui->trigInProp_comboBox->setDisabled(true);
        ui->IntExtTrigMode_But->setChecked(false);
        ui->VsyncTrigMode_But->setChecked(true);
        ui->VsyncTrigMode_But->setEnabled(false);
        ui->IntExtTrigMode_But->setEnabled(false);
        ui->InsBlack_checkbox->setChecked(false);
        ui->trigInProp_comboBox->setCurrentIndex(ExtPos);
    }
    else
    {
        ui->label_Seq_Timing_SplashVSysnc->setText("Image Load Time (us)");
        ui->framePer_lineEdit->setText(numToStr(200000));
        ui->trigInProp_comboBox->setDisabled(false);
        ui->VsyncTrigMode_But->setEnabled(true);
        ui->IntExtTrigMode_But->setEnabled(true);
        ui->IntExtTrigMode_But->setChecked(true);
        ui->VsyncTrigMode_But->setChecked(false);
        ui->InsBlack_checkbox->setChecked(true);
        ui->trigInProp_comboBox->setCurrentIndex(IntTrig);
    }

    emit on_clearAllButton_clicked();
}

