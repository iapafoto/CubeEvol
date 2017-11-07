#include "Creature.h"
#include <chrono> //timing
#include <iostream>
#include <fstream>
#include <math.h>


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


double FREQUENCY = 2.;  //4.0;


void initEmptyVoxelEngine(CVoxelyze* voxelEngine) {
	voxelEngine->enableCollisions();
	voxelEngine->enableFloor();
	voxelEngine->setGravity(9.81f);

		/*
		CVX_Material* pMaterial = voxelEngine->addMaterial(1000000, 1000); //A material with stiffness E=1MPa and density 1000Kg/m^3
		CVX_Material* pMaterialMuscle = voxelEngine->addMaterial(1000000, 1000); //A material with stiffness E=1MPa and density 1000Kg/m^3
		CVX_Material* pMaterialMuscle2 = voxelEngine->addMaterial(1000000, 1000); //A material with stiffness E=1MPa and density 1000Kg/m^3

		pMaterialMuscle->setGlobalDamping(0.1);
		pMaterial->setPoissonsRatio(0.4);
		pMaterial->setStaticFriction(0.8);
		pMaterial->setKineticFriction(0.8);

		pMaterialMuscle->setPoissonsRatio(0.4);
		pMaterialMuscle->setStaticFriction(0.8);
		pMaterialMuscle->setKineticFriction(0.8);

		pMaterialMuscle2->setPoissonsRatio(0.4);
		pMaterialMuscle2->setStaticFriction(0.8);
		pMaterialMuscle2->setKineticFriction(0.8);
		*/

	int NB_MATERIAL = 1 + 2;

	float poisson = .4;
	float damping = .05;
	float dampoungModulus = 2e6; // 1e6 // 10 kPa : mousse - 1 000 GPa (céramiques)
	float staticFriction = .5;
	float kineticFriction = .8;
	float collisionDamping = 0.;

	int density = 500;

	for (int i = 0; i < NB_MATERIAL; i++) {
		voxelEngine->addMaterial(dampoungModulus, density); //A material with stiffness E=1MPa and density 1000Kg/m^3
	}

	CVX_Material* pMaterial;
	char buffer[3];
	for (int i = 0; i < voxelEngine->materialCount(); i++) {
		pMaterial = voxelEngine->material(i);
		pMaterial->setName(itoa(i, buffer, 10));
		pMaterial->setGlobalDamping(damping);
		pMaterial->setPoissonsRatio(poisson);
		pMaterial->setStaticFriction(staticFriction);
		pMaterial->setKineticFriction(kineticFriction);
		pMaterial->setCollisionDamping(collisionDamping);

		if (i == 0) pMaterial->setColor(64, 64, 64);
		if (i == 1) pMaterial->setColor(255, 100, 100);
		if (i == 2) pMaterial->setColor(100, 100, 255);
	}

}
bool haveOtherNeibourg(CVX_Voxel* vx, CVX_Voxel* toRemove) {
	CVX_Voxel* vxn;
	for (int i = 0; i < 6; i++) {
		vxn = vx->adjacentVoxel((CVX_Voxel::linkDirection)i);
		if (vxn != NULL && vxn != toRemove)
			return true;
	}
	return false;
}

bool isRemovable(CVX_Voxel* vx) {
	bool haveNeibourg = false;
	CVX_Voxel* vxn;
	for (int i = 0; i < 6; i++) {
		vxn = vx->adjacentVoxel((CVX_Voxel::linkDirection)i);
		if (vxn != NULL) {
			if (!haveOtherNeibourg(vxn, vx))
				return false;
			haveNeibourg = true;
		}
	}
	return haveNeibourg;
}

// TODO: faire plutot une map des materiaux references par leur nom
int getMaterialId(CVoxelyze* vx0, CVX_Material* mat) {
	for (int i = 0; i < vx0->materialCount(); i++) {
		if (mat == vx0->material(i)) {
			return i;
		}
	}
	return -1;
}

// on double le nombre de voxel en conservant la taille globale
CVoxelyze* divideVXE(CVoxelyze* vx0) {
	CVoxelyze* vxe = new CVoxelyze(vx0->voxelSize() / 2.);

	initEmptyVoxelEngine(vxe);
	// on rajoute les voxels 1 a 1
	for (int i = 0; i < vx0->voxelCount(); i++) {
		CVX_Voxel* vx = vx0->voxel(i);
		int matId = getMaterialId(vx0, vx->material());
		if (matId >= 0) {
			for (int x = 0; x<2; x++)
				for (int y = 0; y<2; y++)
					for (int z = 0; z<2; z++)
						vxe->setVoxel(vxe->material(matId), vx->indexX() * 2 + x, vx->indexY() * 2 + y, vx->indexZ() * 2 + z);
		}
	}
	return vxe;
}

CVoxelyze* cloneVXE(CVoxelyze* vx0, int dx = 0, int dy = 0, int dz = 0) {
	CVoxelyze* vxe = new CVoxelyze(vx0->voxelSize());

	initEmptyVoxelEngine(vxe);
	// on rajoute les voxels 1 a 1
	for (int i = 0; i < vx0->voxelCount(); i++) {
		CVX_Voxel* vx = vx0->voxel(i);
		int matId = getMaterialId(vx0, vx->material());
		if (matId >= 0) {
			vxe->setVoxel(vxe->material(matId), vx->indexX() + dx, vx->indexY() + dy, vx->indexZ() + dz);
		}
	}
	return vxe;
}

//////////////////////////////////////////////////////////////////////


Creature::Creature()
{
	score = -1.;
	voxelEngine = createCreature();
}

Creature::Creature(std::string jsonPath)
{
	score = -1.;
	voxelEngine = new CVoxelyze();
	bool result = voxelEngine->loadJSON(jsonPath.c_str());
	int cpt = voxelEngine->voxelCount();
}

Creature::Creature(CVoxelyze* vx)
{
	score = -1.;
	//if (vx->indexMinZ() < 0) { // sinon on est catapulte par le sol ce qui fausse le resultat
	// on fait coller la creature au sol au demarrage et on la recentre
	vx = cloneVXE(vx, -(vx->indexMinX()+vx->indexMaxX())/2, -(vx->indexMinX() + vx->indexMaxX()) / 2, -vx->indexMinZ());
	//}
	voxelEngine = vx;
}


Creature::~Creature()
{
	if (voxelEngine) delete voxelEngine;
	voxelEngine = NULL;
	if (meshRender) delete meshRender;
	meshRender = NULL;
}

void Creature::clearMemory()
{
	delete voxelEngine;
	if (meshRender) delete meshRender;
}

CVoxelyze* Creature::createCreature() {
	CVoxelyze* voxelEngine = new CVoxelyze(.05);

	initEmptyVoxelEngine(voxelEngine);
	//	pMaterial->setGlobalDamping(0.1);
	CVX_Material* pMaterial = voxelEngine->material(0); //A material with stiffness E=1MPa and density 1000Kg/m^3
	CVX_Material* pMaterialMuscle1 = voxelEngine->material(1); //A material with stiffness E=1MPa and density 1000Kg/m^3
	CVX_Material* pMaterialMuscle2 = voxelEngine->material(2); //A material with stiffness E=1MPa and density 1000Kg/m^3
//	CVX_Material* pMaterialMuscle3 = voxelEngine->material(3); //A material with stiffness E=1MPa and density 1000Kg/m^3
	
	voxelEngine->setVoxel(pMaterial, 0, 0, 0);
//	voxelEngine->setVoxel(pMaterialMuscle1, 0, 0, 1);
//	voxelEngine->setVoxel(pMaterialMuscle2, 0, -1, 1);

//	for (int x=0; x<2; x++)
//		for (int y = 0; y<2; y++)
		//	for (int z = 0; z<2; z++)
//				voxelEngine->setVoxel(pMaterial, x,y,0);

//	voxelEngine->setVoxel(pMaterialMuscle1, 0, 0, 1);
//	voxelEngine->setVoxel(pMaterialMuscle2, 0, -1, 1);
	//	voxelEngine->setVoxel(pMaterialMuscle1, 4, 3, 1);
//	voxelEngine->setVoxel(pMaterialMuscle3, 4, 3, 1);

	return voxelEngine;
}


std::string Creature::getKey() {
	if (key.empty()) {
		CVX_Voxel *v0;
		for (int x = voxelEngine->indexMinX(); x <= voxelEngine->indexMaxX(); x++) {
			key += "|";
			for (int y = voxelEngine->indexMinY(); y <= voxelEngine->indexMaxY(); y++) {
				key += ";";
				for (int z = voxelEngine->indexMinZ(); z <= voxelEngine->indexMaxZ(); z++) {
					key += ",";
					v0 = voxelEngine->voxel(x, y, z);
					if (v0 != NULL) {
						key += v0->material()->name();
					}
				}
			}
		}
	}
	return key;
}

bool Creature::saveJSON(std::string path) {
	return voxelEngine->saveJSON(path.c_str());
}


bool Creature::isSame(Creature* c) {
	CVoxelyze* vx = c->voxelEngine;
	return (getKey() == c->getKey());
}


CVoxelyze* remove(CVoxelyze* vx0, int idx, int idy, int idz) 
{
	CVoxelyze* vxe = new CVoxelyze(vx0->voxelSize());

	initEmptyVoxelEngine(vxe);
	// on rajoute les voxels 1 a 1
	for (int i = 0; i < vx0->voxelCount(); i++) {
		CVX_Voxel* vx = vx0->voxel(i);
		if (vx->indexX() == idx && vx->indexY() == idy && vx->indexZ() == idz)
			continue;
		int matId = getMaterialId(vx0, vx->material());
		if (matId >= 0) {
			vxe->setVoxel(vxe->material(matId), vx->indexX(), vx->indexY(), vx->indexZ());
		}
	}
	return vxe;
}


Creature* Creature::clone() 
{
	voxelEngine->resetTime();

	Creature* newCreature = new Creature(cloneVXE(voxelEngine));
	newCreature->score = score;
	newCreature->key = key; // same structure
	newCreature->parent = parent;
	newCreature->name = name;
	return newCreature;
}

Creature* Creature::divide() 
{
	voxelEngine->resetTime();

	Creature* newCreature = new Creature(divideVXE(voxelEngine));
	newCreature->parent = this;
	return newCreature;
}

Creature* Creature::mutate(int nbMutation) 
{
	Creature *mutan, *creature = this;
	nbMutation = rand() % nbMutation + 1;
	for (int m = 0; m < nbMutation; m++) {
		mutan = creature->mutate();
		if (mutan != NULL) {
			if (creature != this)
				delete creature; // creature intermediaire, on libere la memoire
			creature = mutan;
		}
	}
	creature->parent = this;
	return creature;
}

Creature* Creature::duplicateLayerMutation()
{
	int nbX = (voxelEngine->indexMaxX() - voxelEngine->indexMinX())+1,
		nbY = (voxelEngine->indexMaxY() - voxelEngine->indexMinY())+1,
		nbZ = (voxelEngine->indexMaxZ() - voxelEngine->indexMinZ())+1;
	int col, layer, rnd = rand() % (nbX + nbY + nbZ);
	bool remove = false; // rand() % 2;

	if (rnd < nbX)			  { col = 0; layer = rnd; }
	else if (rnd < nbX + nbY) {	col = 1; layer = rnd-nbX; }
	else					  {	col = 3; layer = rnd - nbX - nbY; }

	CVoxelyze* vxe = new CVoxelyze(voxelEngine->voxelSize());
	int ind[3];
	initEmptyVoxelEngine(vxe);
	// on rajoute les voxels 1 a 1
	for (int i = 0; i < voxelEngine->voxelCount(); i++) {
		CVX_Voxel* vx = voxelEngine->voxel(i);
		int matId = getMaterialId(voxelEngine, vx->material());
		if (matId >= 0) {
			ind[0] = vx->indexX();
			ind[1] = vx->indexY();
			ind[2] = vx->indexZ();
			if (remove) {
				if (ind[col] == layer) continue;
				if (ind[col] > layer) {
					ind[col]--;
				}
			}
			else {
				if (ind[col] == layer) {
					vxe->setVoxel(vxe->material(matId), ind[0], ind[1], ind[2]);
				}
				if (ind[col] >= layer) {
					ind[col]++;
				}
			}
			vxe->setVoxel(vxe->material(matId), ind[0], ind[1], ind[2]);
		}
	}
	return new Creature(vxe);
}

Creature* Creature::flipMutation(int ix, int iy, int iz, int fx, int fy, int fz)
{
	CVoxelyze* vxe = new CVoxelyze(voxelEngine->voxelSize());
	int ind[3];
	initEmptyVoxelEngine(vxe);
	// on rajoute les voxels 1 a 1
	for (int i = 0; i < voxelEngine->voxelCount(); i++) {
		CVX_Voxel* vx = voxelEngine->voxel(i);
		int matId = getMaterialId(voxelEngine, vx->material()); 
		if (matId >= 0) {
			ind[0] = vx->indexX();
			ind[1] = vx->indexY();
			ind[2] = vx->indexZ();
			vxe->setVoxel(vxe->material(matId), fx*ind[ix], fy*ind[iy], fz*ind[iz]);
		}
	}
	return new Creature(vxe);
}

int rand(int max) {	
	return max < 2 ? 0 : rand() % max;
}

std::string Creature::getGenealogie() {
	std::string s = name;
	Creature* p = parent;
	int n = 0;
	while (p != NULL && ++n<6) {
		s = p->getName() + "> " + s;
		p = p->parent;
	}
	return s;
}

Creature* Creature::mutate() 
{
// Inversion des axes -------------
	int flipRandom = rand() % 300;
	if (flipRandom < 1) {
		return flipMutation(2,1,0, 1,1,1);
	} else if (flipRandom < 2) {
		return flipMutation(0,2,1, 1,1,1);
	} else if (flipRandom < 3) {
		return flipMutation(0, -2, -1, 1,1,-1);
	} else if (flipRandom < 4) {
		return flipMutation(0, -2, -1, 1,1,-1);
	}
	else if (flipRandom < 8) {
		duplicateLayerMutation();
	}
// ------------------------------------

	voxelEngine->resetTime();

	CVoxelyze* vxe = cloneVXE(voxelEngine);
	Creature* newCreature;

	float dt0 = voxelEngine->recommendedTimeStep();
	float dt = vxe->recommendedTimeStep();

	int id = rand(vxe->voxelCount());

	CVX_Voxel* vx = vxe->voxel(id);
	int posMut = rand() % 8;
	if (posMut == 0) {
		if (isRemovable(vx)) {
			newCreature = new Creature(remove(vxe, vx->indexX(), vx->indexY(), vx->indexZ()));
			newCreature->parent = this;
			return newCreature;
		}
	}

	//	else {
	int idx = 0, idy = 0, idz = 0;
	switch (posMut) {
		case 1:idz = -1; break;
		case 2:idz = 1; break;
		case 3:idy = -1; break;
		case 4:idy = 1; break;
		case 5:idx = -1; break;
		case 6:idx = 1; break;
		//	7 : apply to voxel 
	}
	// modify or create
	int idMat = rand() % vxe->materialCount();
	CVX_Material* pMatNew = vxe->material(idMat);

	CVX_Voxel* v = vxe->voxel(vx->indexX() + idx, vx->indexY() + idy, vx->indexZ() + idz);
	if (v != NULL && v->material() == pMatNew) {
		// c'est deja le material a cet endroit là => on prend le material suivant pour effectivement faire une mutation
		pMatNew = vxe->material((idMat+1)% vxe->materialCount());
	}
	
	vxe->setVoxel(pMatNew, vx->indexX() + idx, vx->indexY() + idy, vx->indexZ() + idz);
		
//	}
	// soit on lui cree un voisin
	newCreature = new Creature(vxe);
	newCreature->parent = this;
	return newCreature;
}


// duree en secondes
double Creature::simulate(int id, float duration) 
{
	resetTime();

	auto t1 = std::chrono::high_resolution_clock::now();

	double distance = 0.;
	int nb = 0;

	if (score < -.5) {
		glm::vec3 pos0 = getPosition();

		//process to be timed
		float dt = voxelEngine->recommendedTimeStep();
		int nbTry1 =0,nbTry3=0;
		while (time < duration) {
			dt = animate(&nbTry1, &nbTry3);
			if (dt == 0.f)
				break;
			nb++;
		}
	//	std::cout << "nbTry3 ok / nok " << nbTry3 << " " << nbTry1 << "/" << nb;

		glm::vec3 pos1 = getPosition();
		// Distance seulement sur xy
		pos0.z = 0;
		pos1.z = 0;
		score = glm::length(pos1 - pos0);
	}

	auto t2 = std::chrono::high_resolution_clock::now();
	if (parent != NULL && score > parent->getScore()) 
		std::cout << "+";
	else
		std::cout << ".";
	// id << " - (" << nb << ") Simulation of " << duration << "s took: "
	//	<< std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count()
	//	<< " milliseconds\n";

	return score; 
}


double mix(double x, double y, double a) {
	return x + (y - x) * a;
}


float Creature::animate(int* nbTry1, int* nbTry3)
{
	double f = FREQUENCY;
	voxelEngine->material(1)->setExternalScaleFactor(.85f + .15f*cos(M_PI*f*time));
	voxelEngine->material(2)->setExternalScaleFactor(.85f + .15f*cos(M_PI*(1. + f*time)));

	// Recalculate global position
	float dt = voxelEngine->recommendedTimeStep();

//	if (voxelEngine->doTimeStep(dt)) {
//		dt *= 3;
//		(*nbTry3)++;
//	}
//	else {
		voxelEngine->doTimeStep(dt);
//		(*nbTry1)++;
//	}

	time += dt;

	return dt;
}

void Creature::resetTime() {
	time = 0;
	voxelEngine->resetTime();
}

glm::vec3 Creature::getPosition()
{
	glm::vec3 gPos;
	CVX_Voxel* pV;
	int vCount = voxelEngine->voxelCount();
	for (int k = 0; k < vCount; k++) {
		pV = voxelEngine->voxel(k);
		gPos.x += pV->position().getX();
		gPos.y += pV->position().getY();
		gPos.z += pV->position().getZ();
	}
	gPos /= (double)vCount;
	return gPos;
}

void Creature::glInit() 
{
	if (meshRender == NULL) {
		meshRender = new MeshRender(voxelEngine);
		meshRender->generateMesh();
		meshRender->LoadCube();
	}
}

// create buffer and fill it with the points of the triangle

void Creature::glUpdate(GLint attribVert, GLint attribUV, GLint attribColor) 
{
	meshRender->UpdateCube(attribVert, attribUV, attribColor);
}

void Creature::glRender() 
{
	meshRender->RenderCube();
}


bool Creature::sorter(const Creature *l, const Creature *r)
{
	return l->score > r->score;
}


// ...
//niveau1->fils.sort(pred);

// liste.sort(MyGreater());
//struct MyGreater {
//	bool operator() (const Creature &c1, const Creature &c2)
//	{
//		return c1.score > c2.score;
//	};
//};

