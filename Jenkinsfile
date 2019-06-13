pipeline {
    agent any
    options {
	buildDiscarder(logRotator(numToKeepStr: '10'))
    }

    dir('paracuber') {
	stages {
	    stage('Build') {
		steps [
		    cmake
		    cmakeBuild buildType: 'Release', cleanBuild: true, installation: 'InSearchPath', steps: [[withCmake: true]]
		]
	    }
	}	
    }
}
