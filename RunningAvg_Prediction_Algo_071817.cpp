//------------------------------------------------------------------------------
#include "chai3d.h"
//------------------------------------------------------------------------------
using namespace chai3d;
using namespace std;
//------------------------------------------------------------------------------
#ifndef MACOSX
#include "GL/glut.h"
#else
#include "GLUT/glut.h"
#endif
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// GENERAL SETTINGS
//------------------------------------------------------------------------------

// stereo Mode
/*
    C_STEREO_DISABLED:            Stereo is disabled 
    C_STEREO_ACTIVE:              Active stereo for OpenGL NVDIA QUADRO cards
    C_STEREO_PASSIVE_LEFT_RIGHT:  Passive stereo where L/R images are rendered next to each other
    C_STEREO_PASSIVE_TOP_BOTTOM:  Passive stereo where L/R images are rendered above each other
*/
cStereoMode stereoMode = C_STEREO_DISABLED;

// fullscreen mode
bool fullscreen = false;

// mirrored display
bool mirroredDisplay = false;


//------------------------------------------------------------------------------
// DECLARED VARIABLES
//------------------------------------------------------------------------------

// a world that contains all objects of the virtual environment
cWorld* world;

// a camera to render the world in the window display
cCamera* camera;

// a light source to illuminate the objects in the world
cDirectionalLight *light;

// a haptic device handler
cHapticDeviceHandler* handler;

// a pointer to the current haptic device
cGenericHapticDevicePtr hapticDevice;

// a label to display the haptic device model
cLabel* labelHapticDeviceModel;

// a label to display the position [m] of the haptic device
cLabel* labelHapticDevicePosition;

// a global variable to store the position [m] of the haptic device
cVector3d hapticDevicePosition;

// a label to display the rate [Hz] at which the simulation is running
cLabel* labelHapticRate;

// a small sphere (cursor) representing the haptic device 
cShapeSphere* cursor;

// a small sphere representing the predicted position
cShapeSphere* predictIndicator;

// a line representing the velocity vector of the haptic device
cShapeLine* velocity;

// a line representing the average velocity of the haptic device
cShapeLine* avg_velocity;

// flag for using damping (ON/OFF)
bool useDamping = false;

// flag for using force field (ON/OFF)
bool useForceField = true;

// flag to indicate if the haptic simulation currently running
bool simulationRunning = false;

// flag to indicate if the haptic simulation has terminated
bool simulationFinished = true;

// frequency counter to measure the simulation haptic rate
cFrequencyCounter frequencyCounter;

// information about computer screen and GLUT display window
int screenW;
int screenH;
int windowW;
int windowH;
int windowPosX;
int windowPosY;


//------------------------------------------------------------------------------
// DECLARED FUNCTIONS
//------------------------------------------------------------------------------

// callback when the window display is resized
void resizeWindow(int w, int h);

// callback when a key is pressed
void keySelect(unsigned char key, int x, int y);

// callback to render graphic scene
void updateGraphics(void);

// callback of GLUT timer
void graphicsTimer(int data);

// function that closes the application
void close(void);

// main haptics simulation loop
void updateHaptics(void);


//==============================================================================
/*
    Program:   Prediction_Algo

    This application illustrates a threshold based prediction algorithm using
    the Chai3D example program 01-mydevice.cpp as a basis
*/
//==============================================================================

int main(int argc, char* argv[])
{
    //--------------------------------------------------------------------------
    // INITIALIZATION
    //--------------------------------------------------------------------------

    cout << endl;
    cout << "-----------------------------------" << endl;
    cout << "CHAI3D" << endl;
    cout << "Demo: 01-mydevice" << endl;
    cout << "Copyright 2003-2016" << endl;
    cout << "-----------------------------------" << endl << endl << endl;
    cout << "Keyboard Options:" << endl << endl;
    cout << "[1] - Enable/Disable potential field" << endl;
    cout << "[2] - Enable/Disable damping" << endl;
    cout << "[f] - Enable/Disable full screen mode" << endl;
    cout << "[m] - Enable/Disable vertical mirroring" << endl;
    cout << "[x] - Exit application" << endl;
    cout << endl << endl;


    //--------------------------------------------------------------------------
    // OPENGL - WINDOW DISPLAY
    //--------------------------------------------------------------------------

    // initialize GLUT
    glutInit(&argc, argv);

    // retrieve  resolution of computer display and position window accordingly
    screenW = glutGet(GLUT_SCREEN_WIDTH);
    screenH = glutGet(GLUT_SCREEN_HEIGHT);
    windowW = (int)(0.8 * screenH);
    windowH = (int)(0.5 * screenH);
    windowPosY = (screenH - windowH) / 2;
    windowPosX = windowPosY; 

    // initialize the OpenGL GLUT window
    glutInitWindowPosition(windowPosX, windowPosY);
    glutInitWindowSize(windowW, windowH);

    if (stereoMode == C_STEREO_ACTIVE)
        glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE | GLUT_STEREO);
    else
        glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);

    // create display context and initialize GLEW library
    glutCreateWindow(argv[0]);

#ifdef GLEW_VERSION
    // initialize GLEW
    glewInit();
#endif

    // setup GLUT options
    glutDisplayFunc(updateGraphics);
    glutKeyboardFunc(keySelect);
    glutReshapeFunc(resizeWindow);
    glutSetWindowTitle("CHAI3D");

    // set fullscreen mode
    if (fullscreen)
    {
        glutFullScreen();
    }


    //--------------------------------------------------------------------------
    // WORLD - CAMERA - LIGHTING
    //--------------------------------------------------------------------------

    // create a new world.
    world = new cWorld();

    // set the background color of the environment
    world->m_backgroundColor.setBlack();

    // create a camera and insert it into the virtual world
    camera = new cCamera(world);
    world->addChild(camera);

    // position and orient the camera
    camera->set( cVector3d (0.5, 0.0, 0.0),    // camera position (eye)
                 cVector3d (0.0, 0.0, 0.0),    // look at position (target)
                 cVector3d (0.0, 0.0, 1.0));   // direction of the (up) vector

    // set the near and far clipping planes of the camera
    camera->setClippingPlanes(0.01, 10.0);

    // set stereo mode
    camera->setStereoMode(stereoMode);

    // set stereo eye separation and focal length (applies only if stereo is enabled)
    camera->setStereoEyeSeparation(0.01);
    camera->setStereoFocalLength(0.5);

    // set vertical mirrored display mode
    camera->setMirrorVertical(mirroredDisplay);

    // create a directional light source
    light = new cDirectionalLight(world);

    // insert light source inside world
    world->addChild(light);

    // enable light source
    light->setEnabled(true);

    // define direction of light beam
    light->setDir(-1.0, 0.0, 0.0);

    // create a sphere (cursor) to represent the haptic device
    cursor = new cShapeSphere(0.01);

    // create an indicator for the predicted position
    predictIndicator = new cShapeSphere(0.005);

    // insert cursor inside world
    world->addChild(cursor);

    // insert prediction indicator into world
    world->addChild(predictIndicator);

    // create small line to illustrate the velocity of the haptic device
    velocity = new cShapeLine(cVector3d(0,0,0), 
                              cVector3d(0,0,0));

    // create a small line to illustrate the average velocity of the haptic device
    avg_velocity = new cShapeLine(cVector3d(0,0,0),
                                  cVector3d(0,0,0));

    // insert lines inside world
    world->addChild(velocity);
    world->addChild(avg_velocity);


    //--------------------------------------------------------------------------
    // HAPTIC DEVICE
    //--------------------------------------------------------------------------

    // create a haptic device handler
    handler = new cHapticDeviceHandler();

    // get a handle to the first haptic device
    handler->getDevice(hapticDevice, 0);

    // open a connection to haptic device
    hapticDevice->open();

    // calibrate device (if necessary)
    hapticDevice->calibrate();

    // retrieve information about the current haptic device
    cHapticDeviceInfo info = hapticDevice->getSpecifications();

    // display a reference frame if haptic device supports orientations
    if (info.m_sensedRotation == true)
    {
        // display reference frame
        cursor->setShowFrame(true);

        // set the size of the reference frame
        cursor->setFrameSize(0.05);
    }

    // if the device has a gripper, enable the gripper to simulate a user switch
    hapticDevice->setEnableGripperUserSwitch(true);


    //--------------------------------------------------------------------------
    // WIDGETS
    //--------------------------------------------------------------------------

    // create a font
    cFont *font = NEW_CFONTCALIBRI20();

    // create a label to display the haptic device model
    labelHapticDeviceModel = new cLabel(font);
    camera->m_frontLayer->addChild(labelHapticDeviceModel);
    labelHapticDeviceModel->setText(info.m_modelName);

    // create a label to display the position of haptic device
    labelHapticDevicePosition = new cLabel(font);
    camera->m_frontLayer->addChild(labelHapticDevicePosition);
    
    // create a label to display the haptic rate of the simulation
    labelHapticRate = new cLabel(font);
    camera->m_frontLayer->addChild(labelHapticRate);


    //--------------------------------------------------------------------------
    // START SIMULATION
    //--------------------------------------------------------------------------

    // create a thread which starts the main haptics rendering loop
    cThread* hapticsThread = new cThread();
    hapticsThread->start(updateHaptics, CTHREAD_PRIORITY_HAPTICS);

    // setup callback when application exits
    atexit(close);

    // start the main graphics rendering loop
    glutTimerFunc(50, graphicsTimer, 0);
    glutMainLoop();

    // exit
    return (0);
}

//------------------------------------------------------------------------------

void resizeWindow(int w, int h)
{
    windowW = w;
    windowH = h;
}

//------------------------------------------------------------------------------

void keySelect(unsigned char key, int x, int y)
{
    // option ESC: exit
    if ((key == 27) || (key == 'x'))
    {
        exit(0);
    }

    // option 1: enable/disable force field
    if (key == '1')
    {
        useForceField = !useForceField;
        if (useForceField)
            cout << "> Enable force field     \r";
        else
            cout << "> Disable force field    \r";
    }

    // option 2: enable/disable damping
    if (key == '2')
    {
        useDamping = !useDamping;
        if (useDamping)
            cout << "> Enable damping         \r";
        else
            cout << "> Disable damping        \r";
    }

    // option f: toggle fullscreen
    if (key == 'f')
    {
        if (fullscreen)
        {
            windowPosX = glutGet(GLUT_INIT_WINDOW_X);
            windowPosY = glutGet(GLUT_INIT_WINDOW_Y);
            windowW = glutGet(GLUT_INIT_WINDOW_WIDTH);
            windowH = glutGet(GLUT_INIT_WINDOW_HEIGHT);
            glutPositionWindow(windowPosX, windowPosY);
            glutReshapeWindow(windowW, windowH);
            fullscreen = false;
        }
        else
        {
            glutFullScreen();
            fullscreen = true;
        }
    }

    // option m: toggle vertical mirroring
    if (key == 'm')
    {
        mirroredDisplay = !mirroredDisplay;
        camera->setMirrorVertical(mirroredDisplay);
    }
}

//------------------------------------------------------------------------------

void close(void)
{
    // stop the simulation
    simulationRunning = false;

    // wait for graphics and haptics loops to terminate
    while (!simulationFinished) { cSleepMs(100); }

    // close haptic device
    hapticDevice->close();
}

//------------------------------------------------------------------------------

void graphicsTimer(int data)
{
    if (simulationRunning)
    {
        glutPostRedisplay();
    }

    glutTimerFunc(50, graphicsTimer, 0);
}

//------------------------------------------------------------------------------

//function for velocity upper limit
double axisUpperLim (double a, double limit)
{

    if( a > limit){
        a = limit;
    }
    if( a < -limit){
        a = -limit;
    }
    return a;

}

//------------------------------------------------------------------------------

double runningAverage (double prev_a, double a, double operand)
{
    double avg;
    avg = ((prev_a * operand) + a)/(operand +1);
    return avg;
}

//------------------------------------------------------------------------------

void updateGraphics(void)
{
    /////////////////////////////////////////////////////////////////////
    // UPDATE WIDGETS
    /////////////////////////////////////////////////////////////////////

    // update position of label
    labelHapticDeviceModel->setLocalPos(20, windowH - 40, 0);

    // display new position data
    labelHapticDevicePosition->setText(hapticDevicePosition.str(3));

    // update position of label
    labelHapticDevicePosition->setLocalPos(20, windowH - 60, 0);

    // display haptic rate data
    labelHapticRate->setText(cStr(frequencyCounter.getFrequency(), 0) + " Hz");

    // update position of label
    labelHapticRate->setLocalPos((int)(0.5 * (windowW - labelHapticRate->getWidth())), 15);


    /////////////////////////////////////////////////////////////////////
    // RENDER SCENE
    /////////////////////////////////////////////////////////////////////

    // update shadow maps (if any)
    world->updateShadowMaps(false, mirroredDisplay);

    // render world
    camera->renderView(windowW, windowH);

    // swap buffers
    glutSwapBuffers();

    // wait until all GL commands are completed
    glFinish();

    // check for any OpenGL errors
    GLenum err;
    err = glGetError();
    if (err != GL_NO_ERROR) cout << "Error:  %s\n" << gluErrorString(err);
}

//------------------------------------------------------------------------------

void updateHaptics(void)
{
    // initialize frequency counter
    frequencyCounter.reset();

    // simulation in now running
    simulationRunning  = true;
    simulationFinished = false;

    //declare previous linear velocity
    cVector3d prevLinearVelocity;
    prevLinearVelocity.set (0.0,0.0,0.0);

    //declare average linear velocity
    cVector3d avgLinearVelocity;

    //comparative values for previous velocity
    double px;
    double py;
    double pz;

    //comparative values for current velocity
    double cx;
    double cy;
    double cz;

    //average values for velocity
    double avgx;
    double avgy;
    double avgz;

    //limit values for x, y, z
    double limx = .05;
    double limy = .05;
    double limz = .05;

    // main haptic simulation loop
    while(simulationRunning)
    {

        /////////////////////////////////////////////////////////////////////
        //RUNNING AVERAGE
        /////////////////////////////////////////////////////////////////////

        int cnt;
        for( cnt = 0; cnt <= 30; cnt++ ){
            double operand = double(cnt);

            /////////////////////////////////////////////////////////////////////
            // READ HAPTIC DEVICE
            /////////////////////////////////////////////////////////////////////

            // read position
            cVector3d position;
            hapticDevice->getPosition(position);

            // read orientation
            cMatrix3d rotation;
            hapticDevice->getRotation(rotation);

            // read gripper position
            double gripperAngle;
            hapticDevice->getGripperAngleRad(gripperAngle);

            // read linear velocity
            cVector3d linearVelocity;
            hapticDevice->getLinearVelocity(linearVelocity);
            cx = axisUpperLim(linearVelocity.get(0), limx);
            cy = axisUpperLim(linearVelocity.get(1), limy);
            cz = axisUpperLim(linearVelocity.get(2), limz);

            linearVelocity.set(cx, cy, cz);

            px = prevLinearVelocity.get(0);
            py = prevLinearVelocity.get(1);
            pz = prevLinearVelocity.get(2);

            prevLinearVelocity.set(px, py, pz);
            avgx = runningAverage(avgx, cx, operand);
            avgy = runningAverage(avgy, cy, operand);
            avgz = runningAverage(avgz, cz, operand);

            avgLinearVelocity.set(avgx, avgy, avgz);

            //discard linear velocity as jitter based on threshold value
            if ( abs(px-cx) >= .009 )
            {
                linearVelocity = prevLinearVelocity;
            }
            else
            {
                prevLinearVelocity = linearVelocity;
            }
            if ( abs(py-cy) >= .009 )
            {
                linearVelocity = prevLinearVelocity;
            }
            else
            {
                prevLinearVelocity = linearVelocity;
            }
            if ( abs(pz-cz) >= .009 )
            {
                linearVelocity = prevLinearVelocity;
            }
            else
            {
                prevLinearVelocity = linearVelocity;
            }

            // read angular velocity
            cVector3d angularVelocity;
            hapticDevice->getAngularVelocity(angularVelocity);

            // read gripper angular velocity
            double gripperAngularVelocity;
            hapticDevice->getGripperAngularVelocity(gripperAngularVelocity);

            // read user-switch status (button 0)
            bool button0, button1, button2, button3;
            button0 = false;
            button1 = false;
            button2 = false;
            button3 = false;

            hapticDevice->getUserSwitch(0, button0);
            hapticDevice->getUserSwitch(1, button1);
            hapticDevice->getUserSwitch(2, button2);
            hapticDevice->getUserSwitch(3, button3);


            /////////////////////////////////////////////////////////////////////
            // UPDATE 3D CURSOR MODEL
            /////////////////////////////////////////////////////////////////////

            // update arrow
            velocity->m_pointA = position;
            velocity->m_pointB = cAdd(position, linearVelocity);

            avg_velocity->m_pointA = position;
            avg_velocity->m_pointB = cAdd(position, avgLinearVelocity);

            // update position and orientation of cursor
            cursor->setLocalPos(position);
            cursor->setLocalRot(rotation);

            // update predicted position indicator
            predictIndicator->setLocalPos(cAdd(position, linearVelocity));

            // update global variable for graphic display update
            hapticDevicePosition = position;

            // reset predict location if velocity low
            if ( abs(cx) + abs(cy) +abs(cz) < .001){
                predictIndicator->setLocalPos(position);
                cx = 0.0;
                cy = 0.0;
                cz = 0.0;
            }

            // update frequency counter
            frequencyCounter.signal(1);

            if(cnt == 30){
                cnt = 0;
            }

        }

    }
    
    // exit haptics thread
    simulationFinished = true;
}