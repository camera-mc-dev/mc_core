#include "renderer2/renderActions.h"



// ---------------------------------------------------------------------------//
//                 Monolithic draw mesh                                       //
// ---------------------------------------------------------------------------//
Rendering::DrawMeshAction::DrawMeshAction(AbstractRenderer* in_ren, std::string in_name, GLuint shaderID, std::shared_ptr<Mesh> mesh, GLuint texID, Eigen::Vector4f in_baseColour ) :
	RenderAction(in_ren)
{
	shader     = shaderID;
	name       = in_name;
	vao        = mesh->GetVArrayObject();
	vbo        = mesh->GetVBuffer();
	tbo        = mesh->GetTBuffer();
	nbo        = mesh->GetNBuffer();
	vcbo       = mesh->GetVCBuffer();
	fbo        = mesh->GetFBuffer();
	tex        = texID;
	baseColour = in_baseColour;
	
	if( mesh->faces.cols() > 0 )
	{
		hasFaces    = true;
		numElements = mesh->faces.cols();
		numVertsPerElement = mesh->faces.rows();
	}
	else
	{
		hasFaces    = false;
		numElements = mesh->vertices.cols();
		numVertsPerElement = 1;
	}
}

Rendering::DrawMeshAction::~DrawMeshAction()
{
}

void Rendering::DrawMeshAction::Perform(RenderState &renderState)
{
	
// 	cout << "mesh action: " << name << endl;
	
	// enable blending...
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GLenum err;
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "draw mesh pre: " << err << endl;
	}

	// the renderAction has a transformation matrix that tells
	// us where this action is in space, but now we need to know
	// where it is relative to the camera.
	gl44Matrix M = renderState.modelMatrix * L;
	gl44Matrix P = renderState.projMatrix;
	
//	cout << "M: " << endl;
//	cout << M << endl;
//	
//	cout << "L: " << endl;
//	cout << L << endl;
//	
//	cout << "P: " << endl;
//	cout << P << endl;

	// set the shader...
	glUseProgram(shader);

	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "draw mesh use program: " << err << endl;
	}

	// set vao...
	// TODO: understand when and why we would use more than one vao.
	glBindVertexArray(vao);

	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "draw mesh bind array: " << err << endl;
	}

	// provide the model matrix and projection matrix.
	GLuint location;
	location = glGetUniformLocation(shader, "modelMat");
	glUniformMatrix4fv(location, 1, GL_FALSE, M.data() );

	location = glGetUniformLocation(shader, "projMat" );
	glUniformMatrix4fv(location, 1, GL_FALSE, P.data() );

	location = glGetUniformLocation(shader, "baseColour");
	glUniform4fv(location, 1, baseColour.data() );

	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "draw mesh uniforms: " << err << endl;
	}

	// bind the desired texture.
	glBindTexture(GL_TEXTURE_2D, tex);
	
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "draw mesh bind texture: " << err << endl;
	}

	// tell the output where to go, and what the output comes from.
	// glBindFragDataLocation(shader, 0, "fragColor");	// only have an EXT version of this, so maybe we don't need...

	// then we need to supply the vertex attribute information.
	GLint iv = glGetAttribLocation( shader, "vertexCoord" );
	glEnableVertexAttribArray(iv);  // Vertex position.
	
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "draw mesh attribs a: " << err << endl;
	}

	GLint in = glGetAttribLocation( shader, "normal" );
	bool noNormals = true;
	if( in >= 0 )
	{
		noNormals = false;
		glEnableVertexAttribArray(in);  // Vertex normal.
		
		err = glGetError();
		if( err != GL_NO_ERROR )
		{
			cout << "draw mesh attribs b: " << err << endl;
		}
	}
	
	GLint it = glGetAttribLocation( shader, "texCoord" );
	bool noTexture = true;
	if( it >= 0 )
	{
		noTexture = false;
		glEnableVertexAttribArray(it);  // Vertex texture coord.

		err = glGetError();
		if( err != GL_NO_ERROR )
		{
			cout << "draw mesh attribs c: " << err << endl;
		}
	}
	
	GLint ivc = glGetAttribLocation( shader, "vertexColour" );
	bool noVertexColours = true;
	if( ivc >= 0 )
	{
		noVertexColours = false;
		glEnableVertexAttribArray(ivc);  // Vertex colour.
		
		err = glGetError();
		if( err != GL_NO_ERROR )
		{
			cout << "draw mesh attribs d: " << err << endl;
		}
	}
	

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(iv, 4, GL_FLOAT, GL_FALSE, 0, 0);
	
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "draw mesh buffers b: " << err << endl;
	}

	if( !noNormals )
	{
		glBindBuffer(GL_ARRAY_BUFFER, nbo);
		glVertexAttribPointer(in, 4, GL_FLOAT, GL_FALSE, 0, 0);
		
		err = glGetError();
		if( err != GL_NO_ERROR )
		{
			cout << "draw mesh buffers c: " << err << endl;
		}
	}

	if( !noTexture )
	{
		glBindBuffer(GL_ARRAY_BUFFER, tbo);
		glVertexAttribPointer(it, 2, GL_FLOAT, GL_FALSE, 0, 0);
		
		err = glGetError();
		if( err != GL_NO_ERROR )
		{
			cout << "draw mesh buffers d: " << err << endl;
		}
	}
	
	if( !noVertexColours )
	{
		glBindBuffer(GL_ARRAY_BUFFER, vcbo);
		glVertexAttribPointer(ivc, 4, GL_FLOAT, GL_FALSE, 0, 0);
		
		err = glGetError();
		if( err != GL_NO_ERROR )
		{
			cout << "draw mesh buffers d: " << err << endl;
		}
	}
	
	if( hasFaces )
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fbo);
		
		err = glGetError();
		if( err != GL_NO_ERROR )
		{
			cout << "draw mesh buffers e: " << err << endl;
		}
	}
	
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "draw mesh pre: " << err << endl;
	}

	// cout << "ne : " << numElements << endl;
	// cout << "nve: " << numVertsPerElement << endl;
	switch( numVertsPerElement )
	{
		case 1:
			glDrawArrays( GL_POINTS, 0, numElements );
			break;
		case 2:
			glDrawElements(GL_LINES, numElements*2, GL_UNSIGNED_INT, 0);
			break;
		case 3:
			glDrawElements(GL_TRIANGLES, numElements*3, GL_UNSIGNED_INT, 0);
			break;
		default:
			throw std::runtime_error("Mesh render action can only handle points, lines or triangles.");
	}
	
	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "draw mesh post draw: " << err << endl;
	}
	
	glDisableVertexAttribArray(iv);
	if( !noNormals )
		glDisableVertexAttribArray(in);
	if( !noTexture )
		glDisableVertexAttribArray(it);
	if( !noVertexColours )
		glDisableVertexAttribArray(ivc);

	err = glGetError();
	if( err != GL_NO_ERROR )
	{
		cout << "draw mesh post action: " << err << endl;
	}

	glDisable(GL_BLEND);
}
