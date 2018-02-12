#include "../include/particlesystem.h"
#include <omp.h>
#include <time.h>
#include <random>
#include <limits>
unsigned int iteration = 0;
int scenario;

///////////////////////////////////////////////////////////////////////////////
// Constructor
///////////////////////////////////////////////////////////////////////////////
particlesystem::particlesystem() :
    _isGridVisible(false), surfaceThreshold(0.01), gravityVector(0.0,GRAVITY_ACCELERATION,0.0), grid(NULL), nextGrid(NULL), boundary()
{
    loadScenario(INITIAL_SCENARIO);

}

void particlesystem::loadScenario(int newScenario) {
    // remove all particles
    if (grid) delete grid;
    if (nextGrid) delete nextGrid;
    _walls.clear();
    // reset params
    particle::count = 0;
    iteration = 0;
    // create long grid
    boxSize.x = BOX_SIZE*2.0;
    boxSize.y = BOX_SIZE;
    boxSize.z = BOX_SIZE/2.0;
    int gridXRes = (int)ceil(boxSize.x/h);
    int gridYRes = (int)ceil(boxSize.y/h);
    int gridZRes = (int)ceil(boxSize.z/h);
    boundary.createwall(BOX_SIZE, h, _walls);
    grid = new FIELD_3D(gridXRes, gridYRes, gridZRes);
    nextGrid = new FIELD_3D(gridXRes, gridYRes, gridZRes);
    if (newScenario == SCENARIO_DAM) {
        dt = 5.0f/1000.f;
        particleMass = 0.02;
        viscosity = 1.645;
        generateDamParticleSet();
    }
    else if (newScenario == SCENARIO_CUBE) {
        dt = 5.f/1000.f;
        particleMass = 0.02;
        viscosity = 3;
        generateCubeParticleSet();

    }
    else if (newScenario == SCENARIO_FAUCET) {
        dt = 5.f/1000.f;
        particleMass = 0.02;
        viscosity = 1.645;
        generateFaucetParticleSet();
    }
    else if(newScenario == SCENARIO_RAIN) {
        srand(time(NULL));
        //        dt = 5.f/1000.f;
        //        particleMass = 0.01;
        //        viscosity = 15;
        dt = 5.f/1000.f;
        particleMass = 0.02;
        viscosity = 15;
        gravityVector *= 0.01;
        makeItRain();
    }

    updateGrid();

}

void particlesystem::generateDamParticleSet()
{

    // add boundary condition
    const float step = 0.5 * h;
    for(float xPos = 0.5 * (step - boxSize.x) ; xPos <  - 5 * step; xPos += step){
        for(float yPos = 0.5 * (boxSize.y - step); yPos > -0.5 * (boxSize.y - step ); yPos -= step){
            for(float zPos =  0.5 * (step - boxSize.z); zPos < 0.5 * (boxSize.z - 0.5 * step); zPos += step ){
                addParticle(VEC3F(xPos,yPos,zPos));
            }
        }
    }
    cout << "Loaded dam scenario" << endl;
    cout << "Grid size is " << (*grid).xRes() << "x" << (*grid).yRes() << "x" << (*grid).zRes() << endl;
    cout << "Simulating " << particle::count << " particles" << endl;
}

void particlesystem::generateCubeParticleSet()
{

    // add boundary condition
    const float step =  0.5 *h ;
    for(float xPos =   - boxSize.z; xPos < boxSize.z ; xPos += step)
    {
        for(float yPos =  0.5 * (step - boxSize.z) + 0.75 * boxSize.y; yPos < 0.5 * (boxSize.z - step) + 0.75 * boxSize.y; yPos += step)
        {
            for(float zPos =  0.5 * (step - boxSize.z); zPos < 0.5 * (boxSize.z - step); zPos += step )
            {
                addParticle(VEC3F(xPos,yPos,zPos));
            }
        }
    }
    cout << "Loaded cube scenario" << endl;
    cout << "Grid size is " << (*grid).xRes() << "x" << (*grid).yRes() << "x" << (*grid).zRes() << endl;
    cout << "Simulating " << particle::count << " particles" << endl;
}

void particlesystem::generateFaucetParticleSet()
{

    float xPos {0.5f * -boxSize.x};
    float twoPie = 2 * M_PI;
    for(float radius = (boxSize.z - h) / 2; radius >= 0 ; radius -= (h/2) + (h/10) ){
        int numberOfParticles = ((twoPie*radius) / (h/2)) - 3;
        for(int i = 0; i < numberOfParticles; ++i){
            addParticle(VEC3F(xPos+0.015,boxSize.y + radius * std::cos(twoPie * i / numberOfParticles), radius * std::sin(twoPie * i / numberOfParticles)),VEC3F(0.9,-0.9,0));
        }
    }
}

void particlesystem::makeItRain(){

    for(int i = 0; i < 3; ++i)
        addParticle((1.f / 10000) * VEC3F(rand() % static_cast<int>(boxSize.x * 9980) - (boxSize.x * 4990),
                                          boxSize.y * 20000,
                                          rand() % static_cast<int>(boxSize.z * 9980) - (boxSize.z * 4990))
                    );
}

void particlesystem::addParticle(const VEC3F& position, const VEC3F& velocity) {
    particle *part = new particle(position, velocity);
    (*grid)(0,0,0).push_back(*part);
    (*nextGrid)(0,0,0).push_back(*part);
}

void particlesystem::addParticle(const VEC3F& position) {
    addParticle(position, VEC3F());
}

particlesystem::~particlesystem(){
    if (grid) delete grid;
}

void particlesystem::toggleGridVisble() {
    _isGridVisible = !_isGridVisible;
}

void particlesystem::toggleSurfaceVisible() {
    particle::isSurfaceVisible = !particle::isSurfaceVisible;
}

void particlesystem::toggleShowSpash(){
    particle::showSplash = !particle::showSplash;
}

void particlesystem::toggleTumble() {
    _tumble = !_tumble;
}

void particlesystem::toggleGravity() {
    float gravity = gravityVector.magnitude() > 0.0 ? 0 : GRAVITY_ACCELERATION;
    gravityVector = VEC3F(0,gravity,0);
}

void particlesystem::toggleArrows() {
    particle::showArrows = !particle::showArrows;
}

void particlesystem::setGravityVectorWithViewVector(VEC3F viewVector) {
    if (_tumble)
        gravityVector = viewVector * GRAVITY_ACCELERATION;

}

// to update the grid cells particles are located in
// should be called right after particle positions are updated
void particlesystem::updateGrid() {
#pragma omp parallel for
    for(int z = 0; z < grid->zRes(); ++z )
    {
        for(int y = 0; y < grid->yRes(); ++y)
        {
            for(int x = 0; x < grid->xRes(); ++x)
            {

                vector<particle>& particlesNext = (*nextGrid)(x,y,z);
                vector<particle>& particles = (*grid)(x,y,z);
                //On hash avec les données de New
                for (int p = 0; p < particlesNext.size(); p++)
                {

                    particle& particle = particlesNext[p];

                    int newGridCellX = (int)floor((particle.position().x+BOX_SIZE/2.0)/h);
                    int newGridCellY = (int)floor((particle.position().y+BOX_SIZE/2.0)/h);
                    int newGridCellZ = (int)floor((particle.position().z+BOX_SIZE/2.0)/h);
                    newGridCellX = newGridCellX < 0 ? 0 : newGridCellX >= (*grid).xRes() ? (*grid).xRes() - 1 : newGridCellX;
                    newGridCellY = newGridCellY < 0 ? 0 : newGridCellY >= (*grid).yRes() ? (*grid).yRes() - 1 : newGridCellY;
                    newGridCellZ = newGridCellZ < 0 ? 0 : newGridCellZ >= (*grid).zRes() ? (*grid).zRes() - 1 : newGridCellZ;

                    // check if particle has moved
                    if (x != newGridCellX || y != newGridCellY || z != newGridCellZ){
                        // move the particle to the new grid cell
#pragma omp critical
                        {

                            (*grid)(newGridCellX, newGridCellY, newGridCellZ).push_back(std::move(particle));
                            particles[p] = particles.back();
                            particles.pop_back();
                        }
#pragma omp critical
                        {
                            (*nextGrid)(newGridCellX, newGridCellY, newGridCellZ).push_back(std::move(particle));
                            particlesNext[p] = particlesNext.back();
                            particlesNext.pop_back();
                            --p; // important! make sure to redo this index, since a new particle will (probably) be there
                        }
                    }

                }
            }
        }
    }


}

///////////////////////////////////////////////////////////////////////////////
// OGL drawing
///////////////////////////////////////////////////////////////////////////////
void particlesystem::draw()
{
    static VEC3F blackColor(0,0,0);
    static VEC3F blueColor(0,0,1);
    static VEC3F whiteColor(1,1,1);
    static VEC3F greyColor(0.2, 0.2, 0.2);
    static VEC3F lightGreyColor(0.8,0.8,0.8);
    //static VEC3F greenColor(34.0 / 255, 139.0 / 255, 34.0 / 255);
    static float shininess = 10.0;
    // draw the particles
    glEnable(GL_LIGHTING);
    //glMaterialfv(GL_FRONT, GL_DIFFUSE, blueColor);
    glMaterialfv(GL_FRONT, GL_SPECULAR, whiteColor);
    glMaterialfv(GL_FRONT, GL_SHININESS, &shininess);
    //#pragma omp parallel for
    for (int gridCellIndex = 0; gridCellIndex < (*grid).cellCount(); gridCellIndex++)
    {
        vector<particle>& particles = (*grid).data()[gridCellIndex];
        for (int p = 0; p < particles.size(); p++)
        {
            particle& particle = particles[p];
            glMaterialfv(GL_FRONT, GL_DIFFUSE, blueColor);
            particle.draw();
        }
    }
    glDisable(GL_LIGHTING);
    if (_isGridVisible) {
        // draw the grid
        glColor3fv(lightGreyColor);
        //float offset = -BOX_SIZE/2.0+h/2.0;
        for(int z = 0; z < grid->zRes(); ++z )
        {
            for(int y = 0; y < grid->yRes(); ++y)
            {
                for(int x = 0; x < grid->xRes(); ++x)
                {
                    glColor3fv(VEC3F(x,y,z) / VEC3F(grid->xRes()-1,grid->yRes()-1,grid->zRes()-1));
                    glPushMatrix();
                    glTranslated(x*h-boxSize.x/2.0+h/2.0, y*h-boxSize.y/2.0+h/2.0, z*h-boxSize.z/2.0+h/2.0);
                    glutWireCube(h);
                    glPopMatrix();
                }
            }
        }

    }
    glColor3fv(greyColor);
    glPopMatrix();
    glScaled(boxSize.x, boxSize.y, boxSize.z);
    glutWireCube(1.0);
    glPopMatrix();
}

///////////////////////////////////////////////////////////////////////////////
// Verlet integration
///////////////////////////////////////////////////////////////////////////////
void particlesystem::stepVerlet(){
    static long int frameCount = 0;
    accelerationComputation( );
    VEC3F halfVelocity;
    static float dt_over_2 = dt / 2;
#pragma omp parallel for
    for(int z = 0; z < grid->zRes(); ++z )
    {
        for(int y = 0; y < grid->yRes(); ++y)
        {
            for(int x = 0; x < grid->xRes(); ++x)
            {
                vector<particle>& old = grid->operator ()(x,y,z);
                vector<particle>& next = nextGrid->operator ()(x,y,z);
                for(int p = 0; p < next.size(); ++p)
                {
                    particle& nextParticle = next.at(p);
                    particle& oldParticle = old.at(p);
                    //Position and velocity update
                    //halfVelocity = oldParticle.velocity() + (dt_over_2 * oldParticle.acceleration());
                    //nextParticle.setPosition(oldParticle.position() + (dt * halfVelocity));
                    //nextParticle.setVelocity(halfVelocity + (dt_over_2 * nextParticle.acceleration()));
                    nextParticle.setVelocity(oldParticle.velocity() + (nextParticle.acceleration()  * dt));
                    nextParticle.setPosition(oldParticle.position() + nextParticle.velocity() * dt );



                }
            }
        }
    }
    if( _scenario == SCENARIO_FAUCET && particle::count < MAX_PARTICLES && frameCount % 5 == 0)//&& frameCount % 5 == 0
        generateFaucetParticleSet();
    else if( _scenario == SCENARIO_RAIN && particle::count < MAX_PARTICLES && frameCount % 20 == 0)//&& frameCount % 5 == 0
        makeItRain();
    std::swap(grid,nextGrid);
    updateGrid();
    ++frameCount;
}
///////////////////////////////////////////////////////////////////////////////
// Calculate the acceleration of each particle using a grid optimized approach.
// For each particle, only particles in the same grid cell and the (27) neighboring grid cells must be considered,
// since any particle beyond a grid cell distance away contributes no force.
///////////////////////////////////////////////////////////////////////////////
void particlesystem::accelerationComputation() {
    densityAndPressureComputation();
    static float h2 = h*h;
    static float h4 = h2 * h2;
    float nextThreshold = 0.f;
    //Goes through all grid cells, z first for cache coherence
#pragma omp parallel for
    for(int z = 0; z < grid->zRes(); ++z )
    {
        for(int y = 0; y < grid->yRes(); ++y)
        {
            for(int x = 0; x < grid->xRes(); ++x)
            {

                vector<particle>& old = (*grid)(x,y,z);
                vector<particle>& next = (*nextGrid)(x,y,z);

                for( int p = 0; p < old.size(); ++p)
                {
                    particle& nextParticle = next.at(p);
                    particle& oldParticle = old.at(p);
                    nextParticle.clearForce();
                    nextParticle.normal() *= 0;
                    VEC3F gradient;
                    VEC3F laplacian;
                    float coefpi = nextParticle.pressure() / (nextParticle.density() * nextParticle.density());
                    VEC3F curvature;
                    unsigned int numberCloseNeighbor = 0;
                    for(int zz = z - 1; zz <= z + 1; ++zz)
                    {
                        for(int yy = y - 1; yy <= y + 1; ++yy)
                        {
                            for(int xx = x - 1; xx <= x + 1; ++xx)
                            {
                                if( xx >= 0 &&  xx < grid->xRes() && yy >= 0 && yy < grid->yRes() && zz >= 0 && zz < grid->zRes())
                                {
                                    vector<particle>& neighborhood = (*grid)(xx,yy,zz);
                                    vector<particle>& nextNeighborhood = (*nextGrid)(xx,yy,zz);
                                    for(int k = 0; k < neighborhood.size(); ++k){

                                        particle& neighbor = neighborhood.at(k);
                                        particle& nextneighbor = nextNeighborhood.at(k);
                                        if(oldParticle.id() == neighbor.id())
                                            continue;

                                        VEC3F diffPos = oldParticle.position() - neighbor.position();
                                        float distSquared = diffPos.dot(diffPos);
                                        if( h2 <= distSquared )
                                            continue;

                                        if(h2/1.1 >= distSquared)
                                            ++numberCloseNeighbor;

                                        float overDens = (1.f / nextneighbor.density());
                                        float coefpj = nextneighbor.pressure() * overDens * overDens;

                                        VEC3F currentGradient;
                                        WspikyGradient(diffPos,distSquared,currentGradient);
                                        gradient += ( coefpi + coefpj ) * currentGradient;

                                        float visLap = WviscosityLaplacian(distSquared);
                                        laplacian += ( visLap * overDens ) * ( neighbor.velocity() - oldParticle.velocity() );
                                        nextParticle.normal() += (1.f / nextneighbor.density()) * currentGradient;
                                        curvature += overDens * visLap;
                                    }
                                }
                            }
                        }
                    }

                    //next.size() gives less good results
                    nextParticle.splash() = numberCloseNeighbor < 2;
                    nextParticle.flag() |= nextParticle.splash();

                    /* BODY FORCES */
                    //pressure gradient
                    nextParticle.addForce(-1.f * particleMass * gradient * nextParticle.density());
                    //viscosity force
                    nextParticle.addForce(viscosity * particleMass * laplacian);
                    //gravity
                    nextParticle.addForce(gravityVector * nextParticle.density());
                    nextParticle.setAcceleration(nextParticle.force() / nextParticle.density());

                    VEC3F collision;
                    collisionForce(oldParticle,collision);
                    nextParticle.setAcceleration(nextParticle.acceleration() + collision);

                    //Surface tension
                    nextParticle.normal() *= particleMass;
                    float mag = nextParticle.normal().magnitude();
                    nextThreshold += mag;
                    //Here we compute the surface tension then we smooth it in the next function
                    if( nextParticle.flag() = (mag > surfaceThreshold) )
                    {
                        nextParticle.surfaceTension() = (-SURFACE_TENSION * curvature / mag) * nextParticle.normal();
                    }
                }
            }
        }
    }
    smoothTension();
    surfaceThreshold = nextThreshold / static_cast<float>(particle::count);
}

void particlesystem::collisionForce(particle& p, VEC3F& f_collision){

    //Collision with the wall
    for(auto& wall : _walls)
    {
        float inOrOut = wall.getNormal().dot( wall.getPoint() - p.position()) + 0.01;
        //;
        if( inOrOut < 0.00 )
            continue;
        //Bounce direction
        //(50 * particleMass * WALL_K * inOrOut * WALL_DAMPING * p.velocity().dot(wall.getNormal())) * wall.getNormal();
        f_collision += (WALL_DAMPING * p.velocity().dot(wall.getNormal())) * wall.getNormal();
        //Push acceleration
        f_collision += WALL_K * inOrOut * wall.getNormal();

    }
}

void particlesystem::densityAndPressureComputation(){

    static float h2 = h*h;
    //Goes through all grid cells, z first for cache coherence
#pragma omp parallel for
    for(int z = 0; z < grid->zRes(); ++z )
    {
        for(int y = 0; y < grid->yRes(); ++y)
        {
            for(int x = 0; x < grid->xRes(); ++x)
            {

                vector<particle>& old = (*grid)(x,y,z);
                vector<particle>& next = (*nextGrid)(x,y,z);
                //for all the particle in the current cell
                for(int  p = 0 ; p < old.size(); ++p)
                {
                    float newDensity = 0.;
                    particle& nextParticle = next.at(p);
                    particle& oldParticle = old.at(p);
                    for(int zz = z - 1; zz <= z + 1; ++zz)
                    {
                        for(int yy = y - 1; yy <= y + 1; ++yy)
                        {
                            for(int xx = x - 1; xx <= x + 1; ++xx)
                            {
                                if( xx >= 0 &&  xx < grid->xRes() && yy >= 0 && yy < grid->yRes() && zz >= 0 && zz < grid->zRes())
                                {
                                    for(particle& neighbor : (*grid)(xx,yy,zz)){
                                        VEC3F diffPos = neighbor.position() - oldParticle.position();
                                        float distSquared = diffPos.dot(diffPos);
                                        if(distSquared >= h2)
                                            continue;
                                        newDensity += Wpoly6(distSquared);
                                    }
                                }
                            }
                        }
                    }
                    newDensity *= particleMass;
                    //nextParticle.setDensity( newDensity > CRITICAL_DENSITY ? newDensity : REST_DENSITY);
                    nextParticle.setDensity( newDensity);
                    //nextParticle.setPressure(GAS_STIFFNESS * (std::pow(nextParticle.density() / REST_DENSITY, 7) - 1));
                    float press = GAS_STIFFNESS * (nextParticle.density() - REST_DENSITY);
                    nextParticle.setPressure( press > 0 ? press : 0);
                }
            }
        }
    }
}


void particlesystem::smoothTension(){
    static double h2 = h*h;
#pragma omp parallel for
    for(int z = 0; z < grid->zRes(); ++z )
    {
        for(int y = 0; y < grid->yRes(); ++y)
        {
            for(int x = 0; x < grid->xRes(); ++x)
            {

                vector<particle>& old = (*grid)(x,y,z);
                vector<particle>& next = (*nextGrid)(x,y,z);

                for( int p = 0; p < old.size(); ++p)
                {
                    particle nextParticle = next.at(p);
                    particle oldParticle = old.at(p);
                    float totalWeight = 0.f;
                    VEC3F totalTension;

                    for(int zz = z - 1; zz <= z + 1; ++zz)
                    {
                        for(int yy = y - 1; yy <= y + 1; ++yy)
                        {
                            for(int xx = x - 1; xx <= x + 1; ++xx)
                            {
                                if( xx >= 0 &&  xx < grid->xRes() && yy >= 0 && yy < grid->yRes() && zz >= 0 && zz < grid->zRes())
                                {
                                    vector<particle>& neighborhood = (*grid)(xx,yy,zz);
                                    vector<particle>& nextNeighborhood = (*nextGrid)(xx,yy,zz);

                                    for(int k = 0; k < neighborhood.size(); ++k){

                                        particle& neighbor = neighborhood.at(k);
                                        particle& nextneighbor = nextNeighborhood.at(k);

                                        if(oldParticle.id() == neighbor.id())
                                            continue;

                                        VEC3F diffPos = oldParticle.position() - neighbor.position();
                                        float distSquared = diffPos.dot(diffPos);
                                        if( h2 <= distSquared )
                                            continue;
                                        float weight = Wpoly6(distSquared);
                                        totalWeight += weight;
                                        totalTension += nextneighbor.surfaceTension();
                                    }
                                }
                            }
                        }
                    }
                    //Actual surface tension force after being smoothed by the neighborhood
                    nextParticle.setAcceleration( nextParticle.acceleration() + ( totalTension/ totalWeight));
                }
            }
        }
    }

}

inline float particlesystem::Wpoly6(float radiusSquared) {
    static float hSquared = h*h;
    static float coefficient = 315.0/(64.0*M_PI*pow(h,9));
    static float resultat = coefficient * pow(hSquared-radiusSquared, 3);
    return coefficient * pow(hSquared-radiusSquared, 3);
}

inline void particlesystem::Wpoly6Gradient(VEC3F& diffPosition, float radiusSquared, VEC3F& gradient) {
    static float coefficient = -945.0/(32.0*M_PI*pow(h,9));
    static float hSquared = h*h;
    gradient = coefficient * pow(hSquared-radiusSquared, 2) * diffPosition;
}

inline float particlesystem::Wpoly6Laplacian(float radiusSquared) {
    static float coefficient = -945.0/(32.0*M_PI*pow(h,9));
    static float hSquared = h*h;
    return coefficient * (hSquared-radiusSquared) * (3.0*hSquared - 7.0*radiusSquared);
}

void particlesystem::WspikyGradient(VEC3F& diffPosition, float radiusSquared, VEC3F& gradient) {
    static float coefficient = -45.0/(M_PI*pow(h,6));
    float radius = sqrt(radiusSquared);
    gradient = coefficient * pow(h-radius, 2) * diffPosition / radius;
}


float particlesystem::WviscosityLaplacian(float radiusSquared) {
    static float coefficient = 45.0/(M_PI*pow(h,6));
    float radius = sqrt(radiusSquared);
    return coefficient * (h - radius);
}