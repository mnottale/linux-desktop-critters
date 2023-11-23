#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QWindow>
#include <QTimer>
#include <QDesktopWidget>

QMainWindow* mw;
int xstep = 10;
int ystep = 10;
int px = 10;
int py = 10;
int wwidth;
int wheight;
void fire()
{
  px += xstep;
  py += ystep;
  mw->windowHandle()->setPosition(px,py);
  if (px >= wwidth-100)
    xstep = abs(xstep)*-1;
  else if (px <= 30)
    xstep = abs(xstep);
  if (py > wheight-100)
    ystep = abs(ystep)*-1;
  else if (py <= 30)
    ystep = abs(ystep);
  QTimer::singleShot(std::chrono::milliseconds(20), &fire);
}

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
     auto const rec = QApplication::desktop()->screenGeometry();
   wheight = rec.height();
   wwidth = rec.width();
  QPixmap pix("./critter.png");
  
  mw = new QMainWindow();
  mw->setStyleSheet("background:transparent");
  mw->setAttribute(Qt::WA_TranslucentBackground);
  mw->setWindowFlags(Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
  auto* l = new QLabel();
  l->setPixmap(pix);
  mw->setCentralWidget(l);
  mw->show();
  mw->windowHandle()->setPosition(10,10);
  QTimer::singleShot(std::chrono::milliseconds(20), &fire);
  app.exec();
}