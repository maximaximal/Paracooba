pipeline {
    agent any
    options {
	buildDiscarder(logRotator(numToKeepStr: '10'))
    }

    stages {
	stage('Build') {
	    steps {
		dir('paracuber') {
		    sh 'touch build/third_party/cadical-out/build/libcadical.a'
		    cmakeBuild buildType: 'Release', cleanBuild: true, installation: 'InSearchPath', steps: [[withCmake: true]]
		}
	    }
	}
    }	
}
