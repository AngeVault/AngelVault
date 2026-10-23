#include "sfml_widget.h"
#include "CustomWindow.h"

#include <QApplication>

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    CustomWindow window;

    window.setWindowTitle("AngeVault Editor");
    window.setCentralWidget(new SfmlWidget);
    window.resize(920, 780);
    window.show();

    return app.exec();
}