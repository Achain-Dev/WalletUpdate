#include "AChainUpdate.h"

AChainUpdate::AChainUpdate(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	connect(ui.updateAction, &QAction::triggered, this, &AChainUpdate::startUpdate);
}

void AChainUpdate::startUpdate()
{
	AChainUpdater * updater = new AChainUpdater;
	updater->startUpdate();
}