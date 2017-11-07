/*******************************************************************************
Copyright (c) 2015, Jonathan Hiller
To cite academic use of Voxelyze: Jonathan Hiller and Hod Lipson "Dynamic Simulation of Soft Multimaterial 3D-Printed Objects" Soft Robotics. March 2014, 1(1): 88-101.
Available at http://online.liebertpub.com/doi/pdfplus/10.1089/soro.2013.0010

This file is part of Voxelyze.
Voxelyze is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
Voxelyze is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
See <http://www.opensource.org/licenses/lgpl-3.0.html> for license details.
*******************************************************************************/

#include "MeshRender.h"
#include "include/VX_Voxel.h"


//for file output
#include <iostream>
#include <fstream>

#include <GL/glew.h>
//#include <GLFW/glfw3.h>
//#include <glm/glm.hpp>

//link direction to clockwise vertex lookup info:
CVX_Voxel::voxelCorner CwLookup[6][4] = {
	{CVX_Voxel::PNN, CVX_Voxel::PPN, CVX_Voxel::PPP, CVX_Voxel::PNP}, //linkDirection::X_POS
	{CVX_Voxel::NNN, CVX_Voxel::NNP, CVX_Voxel::NPP, CVX_Voxel::NPN}, //linkDirection::X_NEG
	{CVX_Voxel::NPN, CVX_Voxel::NPP, CVX_Voxel::PPP, CVX_Voxel::PPN}, //linkDirection::Y_POS
	{CVX_Voxel::NNN, CVX_Voxel::PNN, CVX_Voxel::PNP, CVX_Voxel::NNP}, //linkDirection::Y_NEG
	{CVX_Voxel::NNP, CVX_Voxel::PNP, CVX_Voxel::PPP, CVX_Voxel::NPP}, //linkDirection::Z_POS
	{CVX_Voxel::NNN, CVX_Voxel::NPN, CVX_Voxel::PPN, CVX_Voxel::PNN}  //linkDirection::Z_NEG
};

MeshRender::MeshRender(CVoxelyze* voxelyzeInstance)
{
	vx = voxelyzeInstance;
	generateMesh();
}

MeshRender::~MeshRender() {
	vertices.clear(); //vx1, vy1, vz1, vx2, vy2, vz2, vx3, ...
	vertexLinks.clear(); //vx1NNN, vx1NNP, [CVX_Voxel::voxelCorner enum order], ... vx2NNN, vx2NNp, ... (null if no link) 
	quads.clear(); //q1v1, q1v2, q1v3, q1v4, q2v1, q2v2, ... (ccw order)
	quadColors.clear(); //q1R, q1G, q1B, q2R, q2G, q2B, ... 
	quadVoxIndices.clear(); //q1n, q2n, q3n, ... 
	quadNormals.clear(); //q1Nx, q1Ny, q1Nz, q2Nx, q2Ny, q2Nz, ... (needs updating with mesh deformation)
	lines.clear(); //l1v1, l1v2, l2v1, l2v2, ..
}

void MeshRender::generateMesh()
{
	vertices.clear();
	vertexLinks.clear();
	quads.clear();
	quadColors.clear();
	quadVoxIndices.clear();
	quadNormals.clear();
	lines.clear();

	int minX = vx->indexMinX();
	int sizeX = vx->indexMaxX()-minX+1;
	int minY = vx->indexMinY();
	int sizeY = vx->indexMaxY()-minY+1;
	int minZ = vx->indexMinZ();
	int sizeZ = vx->indexMaxZ()-minZ+1;

	CArray3D<int> vIndMap; //maps
	vIndMap.setDefaultValue(-1);
	vIndMap.resize(sizeX+1, sizeY+1, sizeZ+1, minX, minY, minZ);
	int vertexCounter = 0;
	
	//for each possible voxel location: (fill in vertices)
	int vCount = vx->voxelCount();
	for (int k=0; k<vCount; k++){
		CVX_Voxel* pV = vx->voxel(k);
		int x=pV->indexX(), y=pV->indexY(), z=pV->indexZ();

		Index3D thisVox(x, y, z);
		for (int i=0; i<6; i++){ //for each direction that a quad face could exist
			if (pV->adjacentVoxel((CVX_Voxel::linkDirection)i)) continue;
			for (int j=0; j<4; j++){ //for each corner of the (exposed) face in this direction
				CVX_Voxel::voxelCorner thisCorner = CwLookup[i][j];
				Index3D thisVertInd3D = thisVox + Index3D(thisCorner&(1<<2)?1:0, thisCorner&(1<<1)?1:0, thisCorner&(1<<0)?1:0);
				int thisInd = vIndMap[thisVertInd3D];


				//if this vertec needs to be added, do it now!
				if (thisInd == -1){ 
					vIndMap[thisVertInd3D] = thisInd = vertexCounter++;
					for (int i=0; i<3; i++) vertices.push_back(0); //will be set on first updateMesh()
				}

				quads.push_back(thisInd); //add this vertices' contribution to the quad
			}
			//quadLinks.push_back(pV);
			quadVoxIndices.push_back(k);
		}
	}

//#ifdef WITH_LINE
	//vertex links: do here to make it the right size all at once and avoid lots of expensive allocations
	vertexLinks.resize(vertexCounter*8, NULL);
	for (int z=minZ; z<minZ+sizeZ+1; z++){ //for each in vIndMap, now.
		for (int y=minY; y<minY+sizeY+1; y++){
			for (int x=minX; x<minX+sizeX+1; x++){
				int thisInd = vIndMap[Index3D(x,y,z)];
				if (thisInd == -1) continue;

				//backwards links
				for (int i=0; i<8; i++){ //check all 8 possible voxels that could be connected...
					CVX_Voxel* pV = vx->voxel(x-(i&(1<<2)?1:0), y-(i&(1<<1)?1:0), z-(i&(1<<0)?1:0));
					if (pV) vertexLinks[8*thisInd + i] = pV;
				}

				//lines
				for (int i=0; i<3; i++){ //look in positive x, y, and z directions
					int isX = (i==0?1:0), isY = (i==1?1:0), isZ = (i==2?1:0);
					int p2Ind = vIndMap[Index3D(x+isX, y+isY, z+isZ)];
					if (p2Ind != -1){ //for x: voxel(x,y,z) (x,y-1,z) (x,y-1,z-1) (x,y,z-1) -- y: voxel(x,y,z) (x-1,y,z) (x-1,y,z-1) (x,y,z-1) -- z: voxel(x,y,z) (x,y-1,z) (x-1,y-1,z) (x-1,y,z)
						if (vx->voxel(x,			y,			z) ||
							vx->voxel(x-isY,		y-isX-isZ,	z) ||
							vx->voxel(x-isY-isZ,	y-isX-isZ,	z-isX-isY) ||
							vx->voxel(x-isZ,		y,			z-isX-isY)) {
							
							lines.push_back(thisInd); lines.push_back(p2Ind);
						}
					}
				}
			}
		}
	}
//#endif

	//the rest... allocate space, but updateMesh will fill them in.
	size_t quadCount = quads.size()/4;

	quadColors.resize(quadCount*3);
	quadNormals.resize(quadCount*3);

	updateMesh();
}

//updates all the modal properties: offsets, quadColors, quadNormals.
void MeshRender::updateMesh(viewColoring colorScheme, CVoxelyze::stateInfoType stateType)
{
	//location
	size_t vCount = vertices.size()/3;
	if (vCount == 0) return;
	for (int i=0; i<vCount; i++){ //for each vertex...
		Vec3D<float> avgPos;
		int avgCount = 0;
		for (int j=0; j<8; j++){
			CVX_Voxel* pV = vertexLinks[8*i+j];
			if (pV){
				avgPos += pV->cornerPosition((CVX_Voxel::voxelCorner)j);
				avgCount++;
			}
		}
		avgPos /= avgCount;
		vertices[3*i] = avgPos.x;
		vertices[3*i+1] = avgPos.y;
		vertices[3*i+2] = avgPos.z;
	}

	//Find a maximum if necessary:
	float minVal = 0, maxVal = 0;
	if (colorScheme == STATE_INFO){
		maxVal = vx->stateInfo(stateType, CVoxelyze::MAX);
		minVal = vx->stateInfo(stateType, CVoxelyze::MIN);
		if (stateType == CVoxelyze::PRESSURE){ //pressure max and min are equal pos/neg
			maxVal = maxVal>-minVal ? maxVal : -minVal;
			minVal = -maxVal;
		}

	}

	//color + normals (for now just pick three vertices, assuming it will be very close to flat...)
	size_t qCount = quads.size()/4;
	if (qCount == 0) return;

	for (int i=0; i<qCount; i++){
		Vec3D<float> v[4];
		for (int j=0; j<4; j++) v[j] = Vec3D<float>(vertices[3*quads[4*i+j]], vertices[3*quads[4*i+j]+1], vertices[3*quads[4*i+j]+2]);
		Vec3D<float> n = ((v[1]-v[0]).Cross(v[3]-v[0]));
		n.Normalize(); //necessary? try glEnable(GL_NORMALIZE)
		quadNormals[i*3] = n.x;
		quadNormals[i*3+1] = n.y;
		quadNormals[i*3+2] = n.z;

		float r=1.0f, g=1.0f, b=1.0f;
		float jetValue = -1.0f;
		switch (colorScheme){
			case MATERIAL:
				r = ((float)vx->voxel(quadVoxIndices[i])->material()->red())/255.0f;
				g = ((float)vx->voxel(quadVoxIndices[i])->material()->green())/255.0f;
				b = ((float)vx->voxel(quadVoxIndices[i])->material()->blue())/255.0f;
				break;
			case FAILURE:
				if (vx->voxel(quadVoxIndices[i])->isFailed()){g=0.0f; b=0.0f;}
				else if (vx->voxel(quadVoxIndices[i])->isYielded()){b=0.0f;}
				break;
			case STATE_INFO:
				switch (stateType) {
				case CVoxelyze::KINETIC_ENERGY: jetValue = vx->voxel(quadVoxIndices[i])->kineticEnergy()/maxVal; break;
				case CVoxelyze::STRAIN_ENERGY: case CVoxelyze::ENG_STRAIN: case CVoxelyze::ENG_STRESS: jetValue = linkMaxColorValue(vx->voxel(quadVoxIndices[i]), stateType) / maxVal; break;
				case CVoxelyze::DISPLACEMENT: jetValue = vx->voxel(quadVoxIndices[i])->displacementMagnitude()/maxVal; break;
				case CVoxelyze::PRESSURE: jetValue = 0.5-vx->voxel(quadVoxIndices[i])->pressure()/(2*maxVal); break;
				default: jetValue = 0;
				}
			break;
		}

		if (jetValue != -1.0f){
			r = jetMapR(jetValue);
			g = jetMapG(jetValue);
			b = jetMapB(jetValue);
		}

		quadColors[i*3] = r;
		quadColors[i*3+1] = g;
		quadColors[i*3+2] = b;
	}
}
float MeshRender::linkMaxColorValue(CVX_Voxel* pV, CVoxelyze::stateInfoType coloring)
{
	float voxMax = -FLT_MAX;
	for (int i=0; i<6; i++){
		float thisVal = -FLT_MAX;
		CVX_Link* pL = pV->link((CVX_Voxel::linkDirection)i);
		if (pL){
			switch (coloring){
				case CVoxelyze::STRAIN_ENERGY: thisVal = pL->strainEnergy(); break;
				case CVoxelyze::ENG_STRESS: thisVal = pL->axialStress(); break;
				case CVoxelyze::ENG_STRAIN: thisVal = pL->axialStrain(); break;
				default: thisVal=0;
			}
		}
		
		if(thisVal>voxMax) voxMax=thisVal;
	}
	return voxMax;
}

void MeshRender::saveObj(const char* filePath)
{
	std::ofstream ofile(filePath);
	ofile << "# OBJ file generated by Voxelyze\n";
	for (int i=0; i<(int)(vertices.size()/3); i++){
		ofile << "v " << vertices[3*i] << " " << vertices[3*i+1] << " " << vertices[3*i+2] << "\n";
	}

	for (int i=0; i<(int)(quads.size()/4); i++){
		ofile << "f " << quads[4*i]+1 << " " << quads[4*i+1]+1 << " " << quads[4*i+2]+1 << " " << quads[4*i+3]+1 << "\n";
	}

	ofile.close();
}



/*
void MeshRender::glPrepareShape()
{

	//quads
	int qCount = quads.size() / 4;
	GLfloat* g_vert_buffer_data = new GLfloat[qCount * 6 * 3];

	//	GLfloat* g_norm_buffer_data = new GLfloat[qCount* 3];
	//	GLfloat* g_color_buffer_data = new GLfloat[qCount * 3];

	//	for (int i = 0; i < qCount*3; i++) {
	//		g_norm_buffer_data[i] = quadNormals[i];
	//		g_color_buffer_data[i] = quadColors[i];
	//	}

	int id[] = { 0,1,2,2,3,0 };
	int n = 0;
	for (int i = 0; i < qCount; i++) {
		for (int j = 0; j < 6; j++) {
			for (int k = 0; k < 3; k++) {
				g_vert_buffer_data[n++] = vertices[3 * quads[4 * i + id[j]] + k];
			}
		}
	}
	
	// Generate 1 buffer, put the resulting identifier in vertexbuffer
	glGenBuffers(1, &vertexbuffer);
	// The following commands will talk about our 'vertexbuffer' buffer
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	// Give our vertices to OpenGL.
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vert_buffer_data), g_vert_buffer_data, GL_STATIC_DRAW);

//	glEnableVertexAttribArray(gProgram->attrib("vert"));
//	glVertexAttribPointer(gProgram->attrib("vert"), 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), NULL);

	// connect the uv coords to the "vertTexCoord" attribute of the vertex shader
	//	glEnableVertexAttribArray(gProgram->attrib("vertTexCoord"));
	//	glVertexAttribPointer(gProgram->attrib("vertTexCoord"), 2, GL_FLOAT, GL_TRUE, 5 * sizeof(GLfloat), (const GLvoid*)(3 * sizeof(GLfloat)));

	// unbind the VAO
	//glBindVertexArray(0);

//	glGenBuffers(1, &colorbuffer);
//	glBindBuffer(GL_ARRAY_BUFFER, colorbuffer);
	//	glBufferData(GL_ARRAY_BUFFER, sizeof(g_color_buffer_data), g_color_buffer_data, GL_STATIC_DRAW);
//	glBufferData(GL_ARRAY_BUFFER, quadColors.size() * sizeof(float), &quadColors[0], GL_STATIC_DRAW);
}


void MeshRender::glDraw()
{
//http://www.opengl-tutorial.org/beginners-tutorials/tutorial-3-matrices/
	
	// Utilisation
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glVertexAttribPointer(
		0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
		3,                  // size
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		0,                  // stride
		(void*)0            // array buffer offset
	);

	
	// 2nd attribute buffer : colors
//	glEnableVertexAttribArray(1);
//	glBindBuffer(GL_ARRAY_BUFFER, colorbuffer);
//	glVertexAttribPointer(
//		1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
//		3,                                // size
//		GL_FLOAT,                         // type
//		GL_FALSE,                         // normalized?
//		0,                                // stride
//		(void*)0                          // array buffer offset
//	);
	
	glDrawArrays(GL_TRIANGLES, 0, (quads.size() / 4) * 6 * 3); 

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	// Swap buffers
	//glfwSwapBuffers(window);
	//glfwPollEvents();




	//lines
//	glLineWidth(1.0);
//	glBegin(GL_LINES);
//	glColor3d(0, 0, 0); //black lines...

//	int lCount = lines.size()/2;
//	for (int i=0; i<lCount; i++) {
//		glVertex3d(vertices[3*lines[2*i]], vertices[3*lines[2*i]+1], vertices[3*lines[2*i]+2]);
//		glVertex3d(vertices[3*lines[2*i+1]], vertices[3*lines[2*i+1]+1], vertices[3*lines[2*i+1]+2]);
//	}
//	glEnd();
}
*/

void MeshRender::LoadCube() {
	// make VAO and VBO
	glGenVertexArrays(1, &gVAO);
	glGenBuffers(1, &gVBO);
	//UpdateCube(1.0);
}

void MeshRender::RenderCube() {
	// bind the VAO (the triangle)
	glBindVertexArray(gVAO);

	// draw the VAO
	size_t qCount = quads.size() / 4  + 1; // +1 for ground !
	glDrawArrays(GL_TRIANGLES, 0, qCount * 2 * 3);
//	glDrawArrays(GL_TRIANGLES, 0, 6 * 2 * 3);

	// unbind the VAO, the program and the texture
	glBindVertexArray(0);
}

void MeshRender::addPoint(std::vector<float>* array, float x, float y, float z, float u, float v, float r, float g, float b) {
	array->push_back(x);
	array->push_back(y);
	array->push_back(z);

	//std::cout << "coord: " << x << "," << y << "," << z;

	array->push_back(u);
	array->push_back(v);

	array->push_back(r);
	array->push_back(g);
	array->push_back(b);
}

void MeshRender::UpdateCube(GLint attribVert, GLint attribUV, GLint attribColor) {
	
	updateMesh(MATERIAL); // STATE_INFO);
//	glPrepareShape();


	// bind the VAO & VBO
	glBindVertexArray(gVAO);
	glBindBuffer(GL_ARRAY_BUFFER, gVBO);

	float r = 0, g = 1, b = 1;
	std::vector<float> data;

	size_t qCount = quads.size() / 4;

// floor
	r = g = b = 0.;
	float z = -vx->voxelSize() / 2.;// -.005;
	
	addPoint(&data, -200., -200, z, -200, -200, r, g, b);
	addPoint(&data,  200., -200, z, 200, -200, r, g, b);
	addPoint(&data,  -200., 200, z, -200, 200, r, g, b);
	addPoint(&data, 200., 200, z, 200, 200, r, g, b);
	addPoint(&data, -200., 200, z, -200, 200, r, g, b);
	addPoint(&data,  200., -200, z, 200, -200, r, g, b);

	for (int i = 0; i < qCount; i++) {

	//	CVX_Voxel* vox = vx->voxel(0,0,0); // TODO trouver la correspondance
	//	CVX_Voxel* vox1 = vox->adjacentVoxel(CVX_Voxel::linkDirection::X_POS);
	// ne pas ajouter le triangle si il y a un voxel de ce coté là		
		r = quadColors[i * 3];
		g = quadColors[i * 3 + 1];
		b = quadColors[i * 3 + 2];

		if (isnan(r) || isnan(g) || isnan(b)) {
			r = g = b = .5 ;
		}

		addPoint(&data, vertices[3 * quads[4 * i]],     vertices[3 * quads[4 * i] + 1],     vertices[3 * quads[4 * i] + 2],     0, 1, r, g, b);
		addPoint(&data, vertices[3 * quads[4 * i + 1]], vertices[3 * quads[4 * i + 1] + 1], vertices[3 * quads[4 * i + 1] + 2], 0, 0, r, g, b);
		addPoint(&data, vertices[3 * quads[4 * i + 2]], vertices[3 * quads[4 * i + 2] + 1], vertices[3 * quads[4 * i + 2] + 2], 1, 0, r, g, b);

		addPoint(&data, vertices[3 * quads[4 * i + 2]], vertices[3 * quads[4 * i + 2] + 1], vertices[3 * quads[4 * i + 2] + 2], 1, 0, r, g, b);
		addPoint(&data, vertices[3 * quads[4 * i + 3]], vertices[3 * quads[4 * i + 3] + 1], vertices[3 * quads[4 * i + 3] + 2], 1, 1, r, g, b);
		addPoint(&data, vertices[3 * quads[4 * i]],     vertices[3 * quads[4 * i] + 1],     vertices[3 * quads[4 * i] + 2],     0, 1, r, g, b);
	}
	
	glBufferData(GL_ARRAY_BUFFER, data.size()*sizeof(float), &data[0], GL_DYNAMIC_DRAW);

	// connect the xyz to the "vert" attribute of the vertex shader
	glEnableVertexAttribArray(attribVert);
	glVertexAttribPointer(attribVert, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), NULL);

	// connect the uv coords to the "vertTexCoord" attribute of the vertex shader
	glEnableVertexAttribArray(attribUV);
	glVertexAttribPointer(attribUV, 2, GL_FLOAT, GL_TRUE, 8 * sizeof(GLfloat), (const GLvoid*)(3 * sizeof(GLfloat)));

	glEnableVertexAttribArray(attribColor);
	glVertexAttribPointer(attribColor, 3, GL_FLOAT, GL_TRUE, 8 * sizeof(GLfloat), (const GLvoid*)(5 * sizeof(GLfloat)));

	// unbind the VAO
	glBindVertexArray(0);
}

